/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/fan.h>
#include "thermalmgmt.h"
#include "fan.h"
#include "thermal_sensor.h"
#include "board_config.h"
#include "smc.h"
#include "sci.h"
#include "scicodes.h"
#include "peci_hub.h"
#include "pwrplane.h"
#include "smchost.h"
#include "espioob_mngr.h"
#include "memops.h"
#include "gpio_ec.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(thermal, CONFIG_THERMAL_MGMT_LOG_LEVEL);

#define FAN_CPU FAN_LEFT
/**
 * DTT threshold default values upon init in degree celsius:
 * - low trip temp = 95c
 * - high trip temp = 100c
 * - temp hysteresis = 2c
 *
 * Note: All values are expressed in centigrades.
 */
#define DTT_LOW_TRIP_DEFAULT			950U
#define	DTT_HIGH_TRIP_DEFAULT			1000U
#define	DTT_TEMP_HYST_DEFAULT			20U

/* PECI is prone to fail during initial boot to S0.
 * Hence allow 1 sec time before reading temperature from peci.
 * Till then report fail safe temperature of 28C.
 */
#define CPU_TEMP_ACCESS_DELAY_SEC		5U

/* In CS, temperature needs to be checked only once every 8 seconds.
 * This is because the CPU is anyway not running and any transaction
 * to query its temperature will wake the CPU.
 */
#define CPU_TEMP_CS_ACCESS_PERIOD_SEC		8U

#define CPU_FAIL_SAFE_TEMPERATURE		28U

/* CPU fail critical temperature value is 72C */
#define CPU_FAIL_CRITICAL_TEMPERATURE		72U

/* GPU fail critical temperature value is 72C */
#define GPU_FAIL_CRITICAL_TEMPERATURE		72U

/* To reduce polling frequency of PCH Temperature over OOB channel, read only
 * once out of 10 times.
 */
#define PCH_TEMP_POLLING_CNT_TIME_DIVISION	10U
#define PCH_TEMP_BUF_SIZE			6U

/*
 * EC Self control fan speed based on CPU temperature info.
 *
 * Simpler approach used here is to control fan with liner profile.
 * For CPU temperature between 0 to 100, fan to also rotate at equivalent speed,
 * but rather than changing speed on every single degree change, it is changed
 * after 8 degrees.
 */
#define GET_FAN_SPEED_FOR_TEMP(temp)		(temp & (~0x7))

struct therm_sensor *therm_sensor_tbl;
struct fan_dev *fan_dev_tbl;
static uint8_t max_fan_dev;
static bool peci_initialized;
/* Using module property, before introducing straps handler */
static bool fan_override;
static bool bios_fan_override;
static bool ec_fan_control;
static uint8_t bios_fan_speed;
static bool fan_duty_cycle_change;
static int cpu_temp = 30;

struct fan_lookup {
	int16_t temp;
	uint8_t duty_cycle;
};

#define FAN_LOOKUP_ENTRY_GET(node_id, prop, idx)	\
	{ DT_PROP_BY_IDX(node_id, prop, idx),		\
	  DT_PROP_BY_IDX(node_id, fan_curve_duty_cycles, idx) },

/* Fan curve, sourced from the 'fan-curve-temps' / 'fan-curve-duty-cycles'
 * devicetree properties (parallel arrays - must be kept the same length).
 * Index 0 is the idle floor for temperatures below the first real
 * threshold; its temp is a sentinel and is not itself a threshold.
 */
static const struct fan_lookup fan_lookup_tbl[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), fan_curve_temps,
			      FAN_LOOKUP_ENTRY_GET)
};

BUILD_ASSERT(DT_PROP_LEN(DT_PATH(zephyr_user), fan_curve_temps) ==
	     DT_PROP_LEN(DT_PATH(zephyr_user), fan_curve_duty_cycles),
	     "fan-curve-temps and fan-curve-duty-cycles must be the same length");

#define FAN_DEVICE_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

const struct device *fan_devices[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), fan_cooling_devices,
			      FAN_DEVICE_GET)
};

#define TEMP_DEVICE_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

const struct device *temp_devices[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), fan_temp_devices,
			      TEMP_DEVICE_GET)
};

/* Temperature filter selection, via devicetree (zephyr,user node). Setting
 * either property selects that filter; setting both is a build error:
 *   fan-temp-filter = <N>;      moving-average window width, in samples
 *                                (this is the default filter; N defaults to
 *                                5 if neither property is set)
 *   fan-temp-ema-alpha = <N>;   selects the EMA filter instead; N is the
 *                                weight given to the newest sample, percent
 *                                (1-100)
 */
BUILD_ASSERT(!(DT_NODE_HAS_PROP(DT_PATH(zephyr_user), fan_temp_filter) &&
	       DT_NODE_HAS_PROP(DT_PATH(zephyr_user), fan_temp_ema_alpha)),
	     "fan-temp-filter and fan-temp-ema-alpha are mutually exclusive");

#define FAN_TEMP_FILTER_WINDOW \
	DT_PROP_OR(DT_PATH(zephyr_user), fan_temp_filter, 5)
#define FAN_TEMP_EMA_ALPHA_PCT \
	DT_PROP_OR(DT_PATH(zephyr_user), fan_temp_ema_alpha, 20)

#if DT_NODE_HAS_PROP(DT_PATH(zephyr_user), fan_temp_ema_alpha)

static float filter_temp(uint16_t temp)
{
	static float ema;
	const float alpha = FAN_TEMP_EMA_ALPHA_PCT / 100.0f;

	ema = (ema == 0) ? (float)temp :
		alpha * (float)temp + (1.0f - alpha) * ema;
	return ema;
}

#else

static float temp_buff[FAN_TEMP_FILTER_WINDOW] = {0};
int buff_idx;
float sum;

static float filter_temp(uint16_t temp)
{
	sum -= temp_buff[buff_idx];
	temp_buff[buff_idx] = (float)temp;
	sum += (float)temp;
	buff_idx = (buff_idx + 1) % FAN_TEMP_FILTER_WINDOW;
	return sum / FAN_TEMP_FILTER_WINDOW;
}

#endif

static uint16_t get_fan_speed_for_temp(uint16_t temp)
{
	static float avg_temp;
	static int idx;
	static bool decreasing;
	static bool increasing = true;
	static uint64_t decreasing_time;
	static uint64_t increasing_time;

	/*
	 * start condition, pick speed and idx
	 */
	if (avg_temp == 0) {
		avg_temp = temp;
		while (idx < ARRAY_SIZE(fan_lookup_tbl) - 1 &&
		       avg_temp >= fan_lookup_tbl[idx+1].temp)
			idx++;
		return fan_lookup_tbl[idx].duty_cycle;
	}

	avg_temp = filter_temp(temp);

	LOG_DBG("filter_temp = %f", (double)avg_temp);
	/*
	 * case where avg_temp is <= lowest
	 */
	if (avg_temp <= (float)fan_lookup_tbl[0].temp) {
		/* if we are on downswing, check the time passed */
		if (decreasing) {
			/* if enough time has passed since going down, zero the index */
			if (k_uptime_get() - decreasing_time >= 55000) {
				idx = 0;
				decreasing = false;
			}
			/* either idx is now 0, or just return the current idx speed */
			return fan_lookup_tbl[idx].duty_cycle;
		}
		/* first time below idx 0 and we were steady-state */
		else {
			decreasing = true;
			increasing = false;
			decreasing_time = k_uptime_get();
			return fan_lookup_tbl[idx].duty_cycle;
		}
	}

	/*
	 * case where avg_temp is >= max
	 */
	if (avg_temp >=
	    (float)fan_lookup_tbl[ARRAY_SIZE(fan_lookup_tbl)-1].temp) {
		if (increasing) {
			/* if enough time has passed since going up, set idx to max */
			if (k_uptime_get() - increasing_time >= 15000) {
				idx = ARRAY_SIZE(fan_lookup_tbl)-1;
				increasing = false;
			}
			/* idx is now max, or just return the current speed */
			return fan_lookup_tbl[idx].duty_cycle;
		}
		/* first time over max and we were steady-state */
		else {
			increasing = true;
			decreasing = false;
			increasing_time = k_uptime_get();
			return fan_lookup_tbl[idx].duty_cycle;
		}
	}

	/*
	 * case where we are above next temp
	 */
	if (idx < ARRAY_SIZE(fan_lookup_tbl) - 1 &&
	    avg_temp >= (float)fan_lookup_tbl[idx+1].temp) {
		if (increasing) {
			if (k_uptime_get() - increasing_time >= 15000) {
				/* find out what temp we have gotten to */
				while (avg_temp >= fan_lookup_tbl[idx+1].temp)
					idx++;
				increasing = false;
			}
			return fan_lookup_tbl[idx].duty_cycle;
		}
		/* we just started going over */
		else {
			increasing = true;
			decreasing = false;
			increasing_time = k_uptime_get();
			return fan_lookup_tbl[idx].duty_cycle;
		}
	}

	/*
	 * case where we are below the lower temp
	 */
	if (idx > 0 && avg_temp <= (float)fan_lookup_tbl[idx-1].temp) {
		if (decreasing) {
			if (k_uptime_get() - decreasing_time >= 55000) {
				while (avg_temp <= fan_lookup_tbl[idx-1].temp)
					idx--;
				decreasing = false;
			}
			return fan_lookup_tbl[idx].duty_cycle;
		}
		/* we just started going down */
		else {
			decreasing = true;
			increasing = false;
			decreasing_time = k_uptime_get();
			return fan_lookup_tbl[idx].duty_cycle;
		}
	}

	/* no change */
	increasing = decreasing = false;
	increasing_time = decreasing_time = k_uptime_get();
	return fan_lookup_tbl[idx].duty_cycle;

}

static void init_fans(void)
{
	int level;

	/* Initialize override */
	level = gpio_read_pin(THERM_STRAP);
	if (level < 0) {
		LOG_ERR("Fail to read thermal strap");
	} else {
		fan_override = !level;
		LOG_WRN("Fan HW override enable: %d", fan_override);
	}

#ifdef CONFIG_THERMAL_FAN_OVERRIDE
	fan_override = true;
	LOG_INF("##### Fan SW override enable: %d #####", fan_override);
#endif

	max_fan_dev = fan_init();

	fan_duty_cycle_change = 1;

}

static void init_therm_sensors(void)
{
}

bool is_fan_controlled_by_host(void)
{
	if (bios_fan_override) {
		return 1;
	}

	if (!is_system_in_acpi_mode()) {
		LOG_DBG("Fan control is over-ridden when not in ACPI mode.");
		return 0;
	}

	return 1;
}

bool is_fan_controlled_by_ec(void)
{
	if (ec_fan_control) {
		return 1;
	}

	return 0;
}

void host_set_bios_fan_override(bool en, uint8_t speed)
{
	LOG_INF("BIOS set over ride control to %d", en);
	bios_fan_override = en;
	bios_fan_speed = speed;
}

static void manage_fan(void)
{
	struct sensor_value temp;
	struct sensor_value max_temp;
	uint8_t fan_duty_cycle;
	int i;

	/* Disable power to fan in S5/4/3 and in CS,
	 * else continue with fan management.
	 */
	if ((pwrseq_system_state() != SYSTEM_S0_STATE) ||
		(smchost_is_system_in_cs())) {
		fan_power_set(false);
		return;
	}
	/* Enable power to fan when system is in S0 and not in CS */
	fan_power_set(true);

	/* Drive the fan curve off the hottest sensor so no single zone can
	 * be masked by cooler readings from the others.
	 */
	for (i = 0; i < ARRAY_SIZE(temp_devices); i++) {
		sensor_sample_fetch_chan(temp_devices[i], SENSOR_CHAN_DIE_TEMP);
		sensor_channel_get(temp_devices[i], SENSOR_CHAN_DIE_TEMP,
				   &temp);

		if (i == 0 || temp.val1 > max_temp.val1)
			max_temp = temp;
	}

	fan_duty_cycle = get_fan_speed_for_temp(max_temp.val1);
	LOG_DBG("Temp read %d, fan state %d", max_temp.val1, fan_duty_cycle);

	for (i = 0; i < ARRAY_SIZE(fan_devices); i++)
		fan_set_cycles(fan_devices[i], fan_duty_cycle);

	fan_update();

}

static void manage_thermal_sensors(void)
{
	thermal_sensors_update();
}

K_TIMER_DEFINE(peci_delay_timer, NULL, NULL);

void peci_start_delay_timer(void)
{
	/* start a one-shot timer for prescribed seconds */
	k_timer_start(&peci_delay_timer, K_SECONDS(CPU_TEMP_ACCESS_DELAY_SEC),
		      K_NO_WAIT);

	LOG_DBG("PECI delay timer started");
}

static void manage_cpu_thermal(void)
{
	int temp, ret;

	/* Manage CPU thermal only in S0 state */
	if (!peci_initialized || k_timer_remaining_get(&peci_delay_timer) ||
	    (pwrseq_system_state() != SYSTEM_S0_STATE)) {
		return;
	}

	/* Read CPU temperature using peci */
	ret = peci_get_temp(CPU, &temp);
	if (ret) {
		LOG_ERR("Failed to get cpu temperature, ret-%x", ret);
		temp = CPU_FAIL_CRITICAL_TEMPERATURE;
	}

	cpu_temp = temp;

	/* Trigger shutdown if temp crosses above critical threshold */
	if (cpu_temp >= g_acpi_tbl.acpi_crit_temp) {
		LOG_DBG("EC thermal shutdown");
		therm_shutdown();
		return;
	}

}

static void manage_pch_temperature(void)
{
}

void thermalmgmt_handle_cs_exit(void)
{
	LOG_DBG("CS Exit: Wake thermal thread from sleep");
	/*In CS mode, thread will be in sleep and may take
	 * up to 'CPU_TEMP_CS_ACCESS_PERIOD_SEC' sec to
	 * wake up. Hence, this trigger to force wake up
	 * & avoid delay
	 */
	wake_task((const char *)THRML_MGMT_TASK_NAME);
}

void thermalmgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;
	int err;

	g_acpi_tbl.acpi_crit_temp = THERM_SHTDWN_THRSD;

	init_fans();
	init_therm_sensors();
	err = peci_init();
	if (!err) {
		peci_initialized = true;
	}

#ifdef CONFIG_EC_FAN_CONTROL
	ec_fan_control = 1;
#endif

	while (true) {
		/* Each thread is aware of CS
		 * Thread uses different sleep time during CS
		 * This required to enter Zephyr-LPM
		 */
		if (smchost_is_system_in_cs()) {
			k_sleep(K_SECONDS(CPU_TEMP_CS_ACCESS_PERIOD_SEC));
		} else {
			k_msleep(normal_period);
		}

		manage_fan();

		/* To achieve infinite C10 residency in connected standby
		 * and ps_on, EC should not send peci cpu & pch temperature
		 * read commands in CS to avoid SOC wake.
		 */
#ifdef CONFIG_PECI_ACCESS_DISABLE_IN_CS
		if (smchost_is_system_in_cs()) {
			continue;
		}
#endif
		manage_thermal_sensors();
		manage_cpu_thermal();
		manage_pch_temperature();
	}
}
