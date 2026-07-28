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

/* Maestro fan curve (Beny profile, confirmed 2026-07-27). Index 0 is the idle floor for
 * temperatures below 60 C; its .temp is a sentinel and is not a real threshold.
 */
static const struct fan_lookup fan_lookup_tbl[]= {
	{0,  50},	/* < 60 C : idle */
	{60, 60},
	{61, 70},
	{62, 80},
	{63, 90},
	{64, 100},
};

const struct device *temp_device = DEVICE_DT_GET(DT_PHANDLE(DT_PATH(zephyr_user), fan_temp_device));

#define FAN_DEVICE_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

const struct device *fan_devices[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), fan_cooling_devices, FAN_DEVICE_GET)
};

K_TIMER_DEFINE(temp_timer, NULL, NULL);

/* Current fan step index into fan_lookup_tbl[]. File-scope (not a function
 * static) so the ramp-up confirm-read in manage_fan can peek the current step
 * before a change is committed.
 */
static int fan_step_idx;

/* Set exactly when temp_timer is running, i.e. when a ramp-down is pending and
 * we are waiting out its 60s window. The two are always armed and cleared
 * together: a set flag with a stopped timer would wedge the gate closed and pin
 * the fan at its current step forever.
 */
static int fan_timer_started;

/* Abandon a pending ramp-down (temp recovered, or the drop just happened). */
static void rampdown_window_cancel(void)
{
	if (fan_timer_started) {
		k_timer_stop(&temp_timer);
		fan_timer_started = 0;
	}
}

static uint16_t get_fan_speed_for_temp(uint16_t temp)
{
	bool index_changed = false;
	const int last = ARRAY_SIZE(fan_lookup_tbl) - 1;

	/* No fast path for "below the lowest active step (60 C)": per Beny's table
	 * "<60C -> 50%" is a ramp-down transition like any other, so it goes through
	 * the 60s window below and lands on index 0 (the 50% idle floor).
	 */

	/* greater than or = highest temp, return max duty cycle */
	if (temp >= fan_lookup_tbl[last].temp) {
		fan_step_idx = last;
		rampdown_window_cancel();
		return fan_lookup_tbl[fan_step_idx].duty_cycle;
	}

	/* Ramp up immediately: allowed every poll (~20s), and evaluated BEFORE the
	 * ramp-down window below so a rising temp always wins over a pending
	 * descent. A step-up during the 60s wait is therefore acted on at that 20s
	 * poll and abandons the descent - the fan never waits out the window to go
	 * faster. A fast spin-up is what keeps the Falcon below 65C. Single-sample
	 * glitches on this edge are rejected by the confirm-read in manage_fan
	 * before we get here.
	 */
	while (fan_step_idx < last && temp >= fan_lookup_tbl[fan_step_idx+1].temp) {
		fan_step_idx++;
		index_changed = true;
	}
	/* if we are going higher, return the new higher duty cycle right away */
	if (index_changed) {
		rampdown_window_cancel();
		return fan_lookup_tbl[fan_step_idx].duty_cycle;
	}

	/* Ramp down. Beny's spec is "on ramp-down change every 1min", so the rate
	 * limit is on how OFTEN the fan may drop, not on how FAR it may drop: when
	 * the window expires we go straight to the table's target for the current
	 * temperature, which may be several steps at once (64C -> 60C is 100% ->
	 * 60%, a single change).
	 *
	 * The window is opened by the first poll that sees the descent and the drop
	 * happens when it expires - never on the 20s poll that detected it. Because
	 * the temp must still be below this step 60s later, the window doubles as
	 * the anti-chatter dead-band that replaced NEGATIVE_HYSTERESIS (now 0).
	 */
	if (fan_step_idx && (temp < fan_lookup_tbl[fan_step_idx].temp -
			     CONFIG_THERMAL_MGMT_NEGATIVE_HYSTERESIS)) {
		if (!fan_timer_started) {
			/* first poll to see it: open the window, hold this step */
			fan_timer_started = 1;
			k_timer_start(&temp_timer, K_SECONDS(60), K_NO_WAIT);
			LOG_DBG("Ramp-down armed at %dC; holding step %d for 60s",
				temp, fan_step_idx);
			return fan_lookup_tbl[fan_step_idx].duty_cycle;
		}

		if (!k_timer_status_get(&temp_timer)) {
			/* window still open: hold */
			return fan_lookup_tbl[fan_step_idx].duty_cycle;
		}

		/* 60s elapsed and temp is still below this step: drop to the table
		 * target for the temperature we are reading right now.
		 */
		while (fan_step_idx && (temp < fan_lookup_tbl[fan_step_idx].temp -
					CONFIG_THERMAL_MGMT_NEGATIVE_HYSTERESIS)) {
			fan_step_idx--;
		}
		rampdown_window_cancel();
		LOG_DBG("Ramp-down fired at %dC; step now %d", temp, fan_step_idx);
		return fan_lookup_tbl[fan_step_idx].duty_cycle;
	}

	/* temp is back inside the current step's band: abandon any pending descent */
	rampdown_window_cancel();

	return fan_lookup_tbl[fan_step_idx].duty_cycle;
}

/* Read the Falcon (switch ASIC) temperature in whole degrees C. */
static int read_falcon_temp(void)
{
	struct sensor_value t;

	sensor_sample_fetch_chan(temp_device, SENSOR_CHAN_DIE_TEMP);
	sensor_channel_get(temp_device, SENSOR_CHAN_DIE_TEMP, &t);

	return t.val1;
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
	uint8_t fan_duty_cycle;
	int temp, cur;
	const int last = ARRAY_SIZE(fan_lookup_tbl) - 1;
	bool hold = false;
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

	temp = read_falcon_temp();
	cur = fan_step_idx;

	/* Ramp-up change point? Confirm it with a second reading before raising
	 * the fan (Beny's "2 consecutive readings" debounce on step entry) so a
	 * single-sample sensor glitch cannot kick the fan up. Every upward step
	 * (including 100% at >=64C) is confirmed, per Beny's table. Ramp-down is
	 * not confirmed - the 60s rate limiter is its "hysteresis" instead.
	 * CONFIRM_MS = 0 disables the re-read.
	 *
	 * Note: the confirm sleeps in the thermal thread, so at a change point the
	 * following manage_cpu_thermal() crit-temp check is delayed by CONFIRM_MS.
	 * Harmless (crit is 103C and the fan is already ramping).
	 */
	if (CONFIG_THERMAL_MGMT_CONFIRM_MS > 0 &&
	    cur < last && temp >= fan_lookup_tbl[cur+1].temp) {
		int t2;

		k_msleep(CONFIG_THERMAL_MGMT_CONFIRM_MS);
		t2 = read_falcon_temp();

		if (t2 >= fan_lookup_tbl[cur+1].temp) {
			/* both readings agree: commit using the fresh reading */
			temp = t2;
		} else {
			/* second read disagrees -> glitch: hold this cycle */
			hold = true;
			LOG_DBG("Ramp-up unconfirmed (crossed %d, t2=%d); hold step %d",
				fan_lookup_tbl[cur+1].temp, t2, cur);
		}
	}

	if (hold)
		fan_duty_cycle = fan_lookup_tbl[cur].duty_cycle;
	else
		fan_duty_cycle = get_fan_speed_for_temp(temp);

	LOG_DBG("Temp read %d, fan state %d", temp, fan_duty_cycle);

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

