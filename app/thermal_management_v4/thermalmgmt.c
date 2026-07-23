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

static const struct fan_lookup fan_lookup_tbl[]= {
	{60, 60},
	{61, 70},
	{62, 80},
	{63, 90},
	{64, 95},
	{65, 100},
};

const struct device *temp_device = DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), fan_temp_device));

#define FAN_DEVICE_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

const struct device *fan_devices[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), fan_cooling_devices, FAN_DEVICE_GET)
};

K_TIMER_DEFINE(temp_timer, NULL, NULL);
static uint16_t get_fan_speed_for_temp(uint16_t temp)
{
	static int idx = 0;
	static int timer_started = 0;
	bool index_changed = false;

	/* less than or = lowest temp, return lowest duty cycle */
	if (temp <= fan_lookup_tbl[0].temp){
		idx = 0;
		k_timer_stop(&temp_timer);
		timer_started = 0;
		return fan_lookup_tbl[idx].duty_cycle;
	}

	/* greater than or = highest temp, return max duty cycle */
	if (temp >= fan_lookup_tbl[ARRAY_SIZE(fan_lookup_tbl)-1].temp) {
		idx = ARRAY_SIZE(fan_lookup_tbl)-1;
		k_timer_stop(&temp_timer);
		timer_started = 0;
		return fan_lookup_tbl[idx].duty_cycle;
	}

	/* Ramp up immediately: allowed every poll (~20s), NOT gated by the
	 * ramp-down timer. A fast spin-up is what keeps the Falcon below 65C.
	 */
	while (temp >= fan_lookup_tbl[idx+1].temp) {
		idx++;
		index_changed = true;
	}
	/* if we are going higher, return the new higher duty cycle right away */
	if (index_changed) {
		return fan_lookup_tbl[idx].duty_cycle;
	}

	/* Ramp down with negative hysteresis: leave a step only once temp falls
	 * CONFIG_THERMAL_MGMT_NEGATIVE_HYSTERESIS degrees below that step's own
	 * entry temperature. Rate-limited to one step per 60s by temp_timer.
	 */
	while ((idx && (temp < fan_lookup_tbl[idx].temp - CONFIG_THERMAL_MGMT_NEGATIVE_HYSTERESIS)) && (!timer_started || k_timer_status_get(&temp_timer)) ) {
		idx--;
		index_changed = true;
	}

	/* if we got a new lower index, start the timer and return new lower duty cycle */	
	if (index_changed) {
		timer_started = 1;
		k_timer_start(&temp_timer, K_SECONDS(60), K_NO_WAIT);
		return fan_lookup_tbl[idx].duty_cycle;
	}

	/* if here, temp is stablizing, if its expired no timers */
	if (k_timer_status_get(&temp_timer)) {
		timer_started = 0;
	}
		
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
	LOG_INF("#################  Fan SW override enable: %d ####################", fan_override);
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

	sensor_sample_fetch_chan(temp_device, SENSOR_CHAN_DIE_TEMP);
	sensor_channel_get(temp_device, SENSOR_CHAN_DIE_TEMP, &temp);

	fan_duty_cycle = get_fan_speed_for_temp(temp.val1);
	LOG_DBG("Temp read %d, fan state %d", temp.val1, fan_duty_cycle);

	for (i = 0; i < ARRAY_SIZE(fan_devices); i++)
		fan_set_cycles(fan_devices[i], fan_duty_cycle);
	
#if !defined(CONFIG_BOARD_MEC172X_ADL_N_CP)
	if (!is_fan_controlled_by_host() || is_fan_controlled_by_ec()) {
		/* EC Self control fan based on CPU thermal info */
		uint8_t cpu_fan_speed = get_fan_speed_for_temp(cpu_temp);
		LOG_INF("%s: board CPU temp: %d, setting duty cycle to %d", __func__,  cpu_temp, cpu_fan_speed);

		if (fan_duty_cycle[FAN_CPU] != cpu_fan_speed) {
			fan_duty_cycle[FAN_CPU] = cpu_fan_speed;
			fan_duty_cycle_change = 1;
		}

		if (fan_duty_cycle[FAN_RIGHT] != cpu_fan_speed) {
			fan_duty_cycle[FAN_RIGHT] = cpu_fan_speed;
			fan_duty_cycle_change = 1;
		}

	}

	/* HW/KConfig override takes precedence over every control method
	 * This is mostly used for PO entry on PO team request
	 */
	if (fan_override) {
		fan_duty_cycle[FAN_CPU] = CONFIG_THERMAL_FAN_OVERRIDE_VALUE;
#if defined(CONFIG_BOARD_MEC172X_AZBEACH) || defined(CONFIG_BOARD_MEC172X_ADL_N)
		fan_duty_cycle[FAN_RIGHT] = CONFIG_THERMAL_FAN_OVERRIDE_VALUE;
#endif
		fan_duty_cycle_change = 1;
	}

	if (fan_duty_cycle_change) {
		fan_duty_cycle_change = 0;

		for (uint8_t idx = 0; idx < max_fan_dev; idx++) {
			fan_set_duty_cycle(idx, fan_duty_cycle[idx]);
		}
	}
#endif
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
	g_acpi_tbl.acpi_crit_temp = THERM_SHTDWN_THRSD;
	int err;

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

