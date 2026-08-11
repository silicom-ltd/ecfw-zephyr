/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/fan.h>
#include <zephyr/drivers/sensor.h>
#include "thermalmgmt.h"
#include "fan_profile_v6.h"
#include "fan.h"
#include "sensors.h"
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

/* CPU fail critical temperature value is 72C */
#define CPU_FAIL_CRITICAL_TEMPERATURE		72U

static bool peci_initialized;
/* Using module property, before introducing straps handler */
static bool fan_override;
static bool bios_fan_override;
static bool ec_fan_control;
static uint8_t bios_fan_speed;
static int cpu_temp = 30;

/* MHO200 adaptive fan speed controller state. Module scope, as in the
 * reference implementation; thermalmgmt_fan_profile_reset() is the only way to
 * clear it.
 */
static struct fan_profile_v6 fan_profile;

/* SW ambient thermistors, left and right. The two LM75s on the switch card,
 * taken from the devicetree in the order they are listed there.
 */
#define AMB_TEMP_SENSOR_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *sw_amb_devices[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), sw_amb_temp_sensors,
			     AMB_TEMP_SENSOR_GET)
};

BUILD_ASSERT(ARRAY_SIZE(sw_amb_devices) == 2,
	"Fan profile expects exactly two SW ambient thermistors, left and right");

/* CPU-side ambient thermistor, on the ADC. It participates in the maximum
 * despite the fan table being labelled SW Ambient.
 */
#if !DT_NODE_EXISTS(DT_NODELABEL(therm0))
#error "Thermal management V6 needs the CPU ambient thermistor (therm0)"
#endif
static const struct device *cpu_amb_device = DEVICE_DT_GET(DT_NODELABEL(therm0));

#define FAN_DEVICE_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

const struct device *fan_devices[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), fan_cooling_devices, FAN_DEVICE_GET)
};

/* Read one ambient thermistor in whole degrees C.
 *
 * sensor_value.val1 is the integer part, so for the non-negative ambients on
 * this board taking val1 is already the floor the profile asks for.
 */
static int read_amb_temp(const struct device *dev, int *temp)
{
	struct sensor_value val;
	int err;

	err = sensor_sample_fetch_chan(dev, SENSOR_CHAN_AMBIENT_TEMP);
	if (err) {
		LOG_WRN("Ambient sensor %s sample failed: %d", dev->name, err);
		return err;
	}

	err = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (err) {
		LOG_WRN("Ambient sensor %s read failed: %d", dev->name, err);
		return err;
	}

	*temp = val.val1;

	return 0;
}

/* floor(max(sw_amb_left, sw_amb_right, ambient)).
 *
 * Sensors that fail to read are left out of the maximum, matching how the
 * hwmon sensor sweep skips them. A cycle where none of the three could be read
 * returns an error instead of a maximum: feeding a made-up low value to the
 * controller would command a fan reduction.
 */
static int read_max_ambient(int *max_sw)
{
	int max = 0;
	int temp;
	int i;
	bool valid = false;

	for (i = 0; i < ARRAY_SIZE(sw_amb_devices); i++) {
		if (read_amb_temp(sw_amb_devices[i], &temp)) {
			continue;
		}
		if (!valid || temp > max) {
			max = temp;
		}
		valid = true;
	}

	if (!read_amb_temp(cpu_amb_device, &temp)) {
		if (!valid || temp > max) {
			max = temp;
		}
		valid = true;
	}

	if (!valid) {
		return -EIO;
	}

	/* The window average relies on integer division truncating the way
	 * floor() does, which only holds while the sum stays non-negative.
	 * Ambient is always positive on this board; clamp rather than trust it.
	 */
	if (max < 0) {
		LOG_WRN("Ambient maximum %dC is negative, clamping to 0", max);
		max = 0;
	}

	*max_sw = max;

	return 0;
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

	fan_init();

	fan_profile_v6_reset(&fan_profile);
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

void thermalmgmt_fan_profile_reset(void)
{
	fan_profile_v6_reset(&fan_profile);
}

static void manage_fan(void)
{
	int max_sw, fan, err, i;

	/* Disable power to fan in S5/4/3 and in CS,
	 * else continue with fan management.
	 */
	if ((pwrseq_system_state() != SYSTEM_S0_STATE) ||
		(smchost_is_system_in_cs())) {
		fan_power_set(false);
		/* The fans are unpowered, so the averaging window and the drop
		 * streak describe a run that has ended. Start the next one from
		 * fresh-process behaviour rather than let it inherit them.
		 */
		thermalmgmt_fan_profile_reset();
		return;
	}
	/* Enable power to fan when system is in S0 and not in CS */
	fan_power_set(true);

	if (read_max_ambient(&max_sw)) {
		LOG_WRN("No ambient sensor read this cycle, fan left at %d%%",
			fan_profile.last_fan);
		return;
	}

	fan = fan_profile_v6_step(&fan_profile, max_sw);

	/* Log the commanded duty cycle on every cycle, including the ones that
	 * command nothing: the measured RPM reported by fan_update() alone
	 * leaves the control decisions unauditable.
	 */
	if (fan == FAN_PROFILE_V6_NO_CHANGE) {
		LOG_INF("Ambient max %dC, avg %dC, down_count %d: hold, fan commanded %d%%",
			max_sw, fan_profile.last_avarage, fan_profile.down_count,
			fan_profile.last_fan);
	} else {
		LOG_INF("Ambient max %dC, avg %dC, down_count %d: fan commanded %d%%",
			max_sw, fan_profile.last_avarage, fan_profile.down_count,
			fan);

		for (i = 0; i < ARRAY_SIZE(fan_devices); i++) {
			err = fan_set_cycles(fan_devices[i], (uint32_t)fan);
			if (err) {
				LOG_WRN("Fan %s set to %d%% failed: %d",
					fan_devices[i]->name, fan, err);
			}
		}
	}

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
	}
}
