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

/* The board's sensor/cooling wiring, described by a
 * Linux/Zephyr-thermal-zones-shaped devicetree node: a "thermal-fan-ctrl-v5"
 * zone with child "trips" (temperature thresholds) and child "cooling-maps"
 * (trip -> cooling device(s), each an independently-tabled fan). See
 * dts/bindings/thermal/thermal-fan-ctrl-v5*.yaml.
 *
 * Trips are evaluated independently (see evaluate_trips()), not as one
 * ordered curve, so unlike most other DT_FOREACH_CHILD uses in this file,
 * cooling-map order doesn't matter here - each map resolves its own trip's
 * temperature via that map's 'trip' phandle.
 */
#define THERMAL_ZONE_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(thermal_fan_ctrl_v5)
#define COOLING_MAPS_NODE DT_CHILD(THERMAL_ZONE_NODE, cooling_maps)

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

/* One (device, min duty, max duty) action per cooling-map cooling-device
 * entry - min/max are cooling-state indices, resolved here into the
 * device's own cooling-levels duty-cycle values at compile time.
 */
struct cooling_action {
	const struct device *dev;
	uint8_t min_duty;
	uint8_t max_duty;
};

#define MAP_TEMP_GET(node_id) DT_PROP(DT_PHANDLE(node_id, trip), temperature),
#define MAP_ACTION_COUNT_GET(node_id) DT_PROP_LEN(node_id, cooling_device),

#define COOLING_ACTION_GET(node_id, prop, idx)				\
	{ DEVICE_DT_GET(DT_PHANDLE(				\
		  DT_PHANDLE_BY_IDX(node_id, prop, idx), fan_devices)),	\
	  DT_PROP_BY_IDX(DT_PHANDLE_BY_IDX(node_id, prop, idx),	\
			  cooling_levels,				\
			  DT_PHA_BY_IDX(node_id, prop, idx, min)),	\
	  DT_PROP_BY_IDX(DT_PHANDLE_BY_IDX(node_id, prop, idx),	\
			  cooling_levels,				\
			  DT_PHA_BY_IDX(node_id, prop, idx, max)) },

#define MAP_ACTIONS_GET(node_id) \
	DT_FOREACH_PROP_ELEM(node_id, cooling_device, COOLING_ACTION_GET)

/* Trip temperature for each cooling-map, resolved via that map's own
 * 'trip' phandle - not the trip's position under the zone's 'trips'
 * container, which this file never walks directly.
 */
static const int16_t map_temps[] = {
	DT_FOREACH_CHILD(COOLING_MAPS_NODE, MAP_TEMP_GET)
};

#define NUM_MAPS ARRAY_SIZE(map_temps)

/* Number of cooling_device entries in each map, and the flat, per-map-
 * grouped list of actions those entries resolve to. A map's own slice of
 * cooling_actions[] starts wherever the running total of earlier maps'
 * counts lands (computed once in evaluate_trips()) and runs for
 * map_action_count[m] entries.
 */
static const uint8_t map_action_count[] = {
	DT_FOREACH_CHILD(COOLING_MAPS_NODE, MAP_ACTION_COUNT_GET)
};

static const struct cooling_action cooling_actions[] = {
	DT_FOREACH_CHILD(COOLING_MAPS_NODE, MAP_ACTIONS_GET)
};

#define TEMP_DEVICE_GET(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

const struct device *temp_devices[] = {
	DT_FOREACH_PROP_ELEM(THERMAL_ZONE_NODE, thermal_sensors,
			      TEMP_DEVICE_GET)
};

/* Temperature filter selection, via the thermal-fan-ctrl-v5 zone node.
 * Setting either property selects that filter; setting both is a build
 * error:
 *   fan-temp-filter = <N>;      moving-average window width, in samples
 *                                (this is the default filter; N defaults to
 *                                5 if neither property is set)
 *   fan-temp-ema-alpha = <N>;   selects the EMA filter instead; N is the
 *                                weight given to the newest sample, percent
 *                                (1-100)
 */
BUILD_ASSERT(!(DT_NODE_HAS_PROP(THERMAL_ZONE_NODE, fan_temp_filter) &&
	       DT_NODE_HAS_PROP(THERMAL_ZONE_NODE, fan_temp_ema_alpha)),
	     "fan-temp-filter and fan-temp-ema-alpha are mutually exclusive");

#define FAN_TEMP_FILTER_WINDOW \
	DT_PROP_OR(THERMAL_ZONE_NODE, fan_temp_filter, 5)
#define FAN_TEMP_EMA_ALPHA_PCT \
	DT_PROP_OR(THERMAL_ZONE_NODE, fan_temp_ema_alpha, 20)

BUILD_ASSERT(FAN_TEMP_EMA_ALPHA_PCT >= 1 && FAN_TEMP_EMA_ALPHA_PCT <= 100,
	     "fan-temp-ema-alpha must be between 1 and 100");

#if DT_NODE_HAS_PROP(THERMAL_ZONE_NODE, fan_temp_ema_alpha)

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

/* Apply every cooling_actions[] entry belonging to map m: 'above' selects
 * each action's max_duty (temperature at/above this map's trip) or
 * min_duty (temperature below it).
 */
static void apply_map(const uint16_t *map_action_offset, int m, bool above)
{
	int start = map_action_offset[m];
	int end = start + map_action_count[m];
	int k;

	for (k = start; k < end; k++) {
		const struct cooling_action *action = &cooling_actions[k];
		uint8_t duty = above ? action->max_duty : action->min_duty;

		fan_set_cycles(action->dev, duty);
	}
}

/* Every cooling-map's trip is evaluated independently against the current
 * temperature, each with its own up/down debounce - there is no shared
 * "current step" the way a single ordered curve would have. On the very
 * first call, every map snaps directly to its correct state (no debounce,
 * and the raw temperature is used instead of the filter, matching the
 * cold-start handling the moving-average/EMA filters can't do on their
 * own - see filter_temp()).
 */
static void evaluate_trips(uint16_t temp)
{
	static bool initialized;
	static uint16_t map_action_offset[NUM_MAPS];
	static bool map_active[NUM_MAPS];
	static bool map_pending_up[NUM_MAPS];
	static bool map_pending_down[NUM_MAPS];
	static uint64_t map_pending_since[NUM_MAPS];
	uint64_t now_ms = k_uptime_get();
	float avg_temp;
	int m;

	if (!initialized) {
		uint16_t offset = 0;

		for (m = 0; m < NUM_MAPS; m++) {
			map_action_offset[m] = offset;
			offset += map_action_count[m];
		}
	}

	avg_temp = initialized ? filter_temp(temp) : (float)temp;

	LOG_DBG("filter_temp = %f", (double)avg_temp);

	for (m = 0; m < NUM_MAPS; m++) {
		bool above = avg_temp >= (float)map_temps[m];

		if (!initialized) {
			map_active[m] = above;
			apply_map(map_action_offset, m, above);
			continue;
		}

		if (above == map_active[m]) {
			/* Steady relative to this trip - drop any pending
			 * debounce so a brief wobble doesn't carry into the
			 * next real crossing.
			 */
			map_pending_up[m] = false;
			map_pending_down[m] = false;
			continue;
		}

		if (above) {
			if (!map_pending_up[m]) {
				map_pending_up[m] = true;
				map_pending_down[m] = false;
				map_pending_since[m] = now_ms;
			} else if (now_ms - map_pending_since[m] >= 15000) {
				map_active[m] = true;
				map_pending_up[m] = false;
				apply_map(map_action_offset, m, true);
			}
		} else {
			if (!map_pending_down[m]) {
				map_pending_down[m] = true;
				map_pending_up[m] = false;
				map_pending_since[m] = now_ms;
			} else if (now_ms - map_pending_since[m] >= 55000) {
				map_active[m] = false;
				map_pending_down[m] = false;
				apply_map(map_action_offset, m, false);
			}
		}
	}

	initialized = true;
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

	/* Drive the trips off the hottest sensor so no single zone can be
	 * masked by cooler readings from the others.
	 */
	for (i = 0; i < ARRAY_SIZE(temp_devices); i++) {
		sensor_sample_fetch_chan(temp_devices[i], SENSOR_CHAN_DIE_TEMP);
		sensor_channel_get(temp_devices[i], SENSOR_CHAN_DIE_TEMP,
				   &temp);

		if (i == 0 || temp.val1 > max_temp.val1)
			max_temp = temp;
	}

	LOG_DBG("Temp read %d", max_temp.val1);
	evaluate_trips(max_temp.val1);

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
