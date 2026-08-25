/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include <zephyr/drivers/adc/current_sense_amplifier.h>

#include "board_config.h"
#include "sensors.h"
#include "sensors_cp.h"


LOG_MODULE_REGISTER(swsens_cp, CONFIG_SW_SENSOR_LOG_LEVEL);

#define SW_SENSOR_DBG

extern struct hwmon_sram *hwmon_data;

struct sw_sens_info {
	const struct device * dev;
	enum sensor_channel   chan; /* param2: get channel */
	char                * name;
	bool                  valid; /* set by _sw_sensors_probe(): device responds on the bus */
};

static struct sw_sens_info sw_thermal_sensors[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), sw_die_temp_sensors, DIE_TEMP_SENSOR_DECLARE)
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), sw_amb_temp_sensors, AMB_TEMP_SENSOR_DECLARE)
};

static struct sw_sens_info sw_voltage_sensors[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), sw_volt_sensors, VOLT_SENSOR_DECLARE)
};

static struct sw_sens_info sw_current_sensors[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), sw_curr_sensors, CURR_SENSOR_DECLARE)
};

static struct sw_sens_info sw_power_sensors[] = {
	DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), sw_powr_sensors, POWR_SENSOR_DECLARE)
};

BUILD_ASSERT(ARRAY_SIZE(sw_thermal_sensors) == SW_THERMAL_SENSOR_NUM,
	"Invalid size of sw_thermal_sensors");
BUILD_ASSERT(ARRAY_SIZE(sw_voltage_sensors) == SW_VOLTAGE_SENSOR_NUM,
	"Invalid size of sw_voltage_sensors");
BUILD_ASSERT(ARRAY_SIZE(sw_current_sensors) == SW_CURRENT_SENSOR_NUM,
	"Invalid size of sw_current_sensors");
BUILD_ASSERT(ARRAY_SIZE(sw_power_sensors)   == SW_POWER_SENSOR_NUM,
	"Invalid size of sw_power_sensors");

static void hwmon_sdata_update(struct hwmon_sdata *sdata, struct sensor_value *sens_value)
{
	uint32_t ret_value;
	uint16_t multiplier = 0;

	ret_value = (sens_value->val1 * 1000) + (sens_value->val2 / 1000);

	while ((ret_value >> multiplier) & 0xFFFF0000)
		multiplier++;

	sdata->multiplier = multiplier;
	sdata->mon_in = ret_value >> sdata->multiplier;

	if (sdata->mon_in > sdata->mon_max)
		sdata->mon_max = sdata->mon_in;
	if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
		sdata->mon_min = sdata->mon_in;
}

/*
 * Not every devicetree-declared sensor is populated on every board revision
 * (e.g. R200 MHO-150/180 swaps some MPQ8785 rails for an MPQ8655). Probe each
 * declared sensor once so absent ones are skipped by later periodic updates
 * instead of spamming a fetch error every cycle.
 */
static void _sw_sensors_probe(struct sw_sens_info * sens_info, int num_sensors,
	const char * label)
{
	int i, err, present = 0;
	struct sensor_value sens_val;

	for (i = 0; i < num_sensors; i++) {
		if (!device_is_ready(sens_info[i].dev)) {
			LOG_WRN("SW Sensor %s: device not ready, marking absent",
				sens_info[i].name);
			sens_info[i].valid = false;
			continue;
		}

		err = sensor_sample_fetch(sens_info[i].dev);
		if (!err)
			err = sensor_channel_get(sens_info[i].dev, sens_info[i].chan, &sens_val);

		sens_info[i].valid = (err == 0);
		if (sens_info[i].valid) {
			present++;
		} else {
			LOG_WRN("SW Sensor %s: not responding (err %d), marking absent",
				sens_info[i].name, err);
		}
	}

	LOG_INF("SW %s sensors: %d/%d present", label, present, num_sensors);
}

static void _sw_sensors_update(struct sw_sens_info * sens_info, int num_sensors,
	struct hwmon_sdata * hwmon)
{
	int i, err;
	struct sensor_value sens_val;

	for (i = 0; i < num_sensors; i++) {
		if (!sens_info[i].valid)
			continue;

		err = sensor_sample_fetch(sens_info[i].dev);
		if (err) {
			LOG_ERR("SW Sensor %s sample failed: %d", sens_info[i].name, err);
			continue;
		}

		sensor_channel_get(sens_info[i].dev, sens_info[i].chan, &sens_val);
		if (err) {
			LOG_ERR("SW Sensor %s chan %d read failed: %d", sens_info[i].name,
				sens_info[i].chan, err);
			continue;
		}

		hwmon_sdata_update(&hwmon[i], &sens_val);

#if (CONFIG_SW_SENSOR_LOG_LEVEL >= LOG_LEVEL_DBG)
		uint32_t final_val = hwmon[i].mon_in << hwmon[i].multiplier;
		LOG_INF(" %16s@hwmon[%02ld]: %3d.%03d (IN 0x%04x MUL %d)%s", sens_info[i].name,
			HWMON_SRAM_ENTRY_IDX(&hwmon[i], hwmon_data),
			final_val/1000, final_val % 1000,
			hwmon[i].mon_in, hwmon[i].multiplier,
			hwmon[i].multiplier > 1 ? "(*)" : "");
		//LOG_INF(" %26s: %3d.%d", "", sens_val.val1, sens_val.val2);
#endif
	}
}

void sw_sensors_hwmon_setting(void)
{
	int i, num_sensors;

	__ASSERT(hwmon_data != NULL, "hwmon_data is NULL");

	LOG_INF("The number of SW thermal sensors is %d", SW_THERMAL_SENSOR_NUM);
	LOG_INF("The number of SW voltage sensors is %d", SW_VOLTAGE_SENSOR_NUM);
	LOG_INF("The number of SW current sensors is %d", SW_CURRENT_SENSOR_NUM);
	LOG_INF("The number of SW power sensors is %d", SW_POWER_SENSOR_NUM);

	_sw_sensors_probe(sw_thermal_sensors, ARRAY_SIZE(sw_thermal_sensors), "thermal");
	_sw_sensors_probe(sw_voltage_sensors, ARRAY_SIZE(sw_voltage_sensors), "voltage");
	_sw_sensors_probe(sw_current_sensors, ARRAY_SIZE(sw_current_sensors), "current");
	_sw_sensors_probe(sw_power_sensors, ARRAY_SIZE(sw_power_sensors), "power");

	num_sensors = ARRAY_SIZE(sw_thermal_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_thermal[i], hwmon_temp);
		LOG_INF("SW SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->sw_mon_thermal[i], hwmon_data),
			sw_thermal_sensors[i].name,
			sw_thermal_sensors[i].valid ? "" : " (absent)");
	}

	num_sensors = ARRAY_SIZE(sw_voltage_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_voltage[i], hwmon_in);

		LOG_INF("SW SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->sw_mon_voltage[i], hwmon_data),
			sw_voltage_sensors[i].name,
			sw_voltage_sensors[i].valid ? "" : " (absent)");
	}

	num_sensors = ARRAY_SIZE(sw_current_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_current[i], hwmon_curr);

		LOG_INF("SW SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->sw_mon_current[i], hwmon_data),
			sw_current_sensors[i].name,
			sw_current_sensors[i].valid ? "" : " (absent)");
	}

	num_sensors = ARRAY_SIZE(sw_power_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_power[i], hwmon_power);

		LOG_INF("SW SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->sw_mon_power[i], hwmon_data),
			sw_power_sensors[i].name,
			sw_power_sensors[i].valid ? "" : " (absent)");
	}
}

void sw_thermal_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(sw_thermal_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_thermal),
		"Invalid size of hwmon sw_mon_thermal");

	_sw_sensors_update(sw_thermal_sensors, ARRAY_SIZE(sw_thermal_sensors),
		hwmon_data->sw_mon_thermal);
}

void sw_voltage_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(sw_voltage_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_voltage),
		"Invalid size of hwmon sw_mon_voltage");

	_sw_sensors_update(sw_voltage_sensors, ARRAY_SIZE(sw_voltage_sensors),
		hwmon_data->sw_mon_voltage);
}

void sw_current_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(sw_current_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_current),
		"Invalid size of hwmon sw_mon_current");

	_sw_sensors_update(sw_current_sensors, ARRAY_SIZE(sw_current_sensors),
		hwmon_data->sw_mon_current);
}

void sw_power_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(sw_power_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_power),
		"Invalid size of hwmon sw_mon_power");

	_sw_sensors_update(sw_power_sensors, ARRAY_SIZE(sw_power_sensors),
		hwmon_data->sw_mon_power);
}

void sw_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(sw_thermal_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_thermal),
		"Invalid size of hwmon sw_mon_thermal");
	__ASSERT(ARRAY_SIZE(sw_voltage_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_voltage),
		"Invalid size of hwmon sw_mon_voltage");
	__ASSERT(ARRAY_SIZE(sw_current_sensors) == ARRAY_SIZE(hwmon_data->sw_mon_current),
		"Invalid size of hwmon sw_mon_current");

	_sw_sensors_update(sw_thermal_sensors, ARRAY_SIZE(sw_thermal_sensors),
		hwmon_data->sw_mon_thermal);

	_sw_sensors_update(sw_voltage_sensors, ARRAY_SIZE(sw_voltage_sensors),
		hwmon_data->sw_mon_voltage);

	_sw_sensors_update(sw_current_sensors, ARRAY_SIZE(sw_current_sensors),
		hwmon_data->sw_mon_current);

	_sw_sensors_update(sw_power_sensors, ARRAY_SIZE(sw_power_sensors),
		hwmon_data->sw_mon_power);
}
