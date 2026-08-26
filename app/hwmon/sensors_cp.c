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


LOG_MODULE_REGISTER(boardsens_cp, CONFIG_BOARD_SENSOR_LOG_LEVEL);

#define BOARD_SENSOR_DBG

extern struct hwmon_sram *hwmon_data;

struct board_sens_info {
	const struct device * dev;
	enum sensor_channel   chan; /* param2: get channel */
	char                * name;
	bool                  valid; /* set by _board_sensors_probe(): device responds on the bus */
};

static struct board_sens_info board_thermal_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_die_temp_sensors, DIE_TEMP_SENSOR_DECLARE)
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_amb_temp_sensors, AMB_TEMP_SENSOR_DECLARE)
};

static struct board_sens_info board_voltage_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_volt_sensors, VOLT_SENSOR_DECLARE)
};

static struct board_sens_info board_current_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_curr_sensors, CURR_SENSOR_DECLARE)
};

static struct board_sens_info board_power_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_powr_sensors, POWR_SENSOR_DECLARE)
};

BUILD_ASSERT(ARRAY_SIZE(board_thermal_sensors) == BOARD_THERMAL_SENSOR_NUM,
	"Invalid size of board_thermal_sensors");
BUILD_ASSERT(ARRAY_SIZE(board_voltage_sensors) == BOARD_VOLTAGE_SENSOR_NUM,
	"Invalid size of board_voltage_sensors");
BUILD_ASSERT(ARRAY_SIZE(board_current_sensors) == BOARD_CURRENT_SENSOR_NUM,
	"Invalid size of board_current_sensors");
BUILD_ASSERT(ARRAY_SIZE(board_power_sensors)   == BOARD_POWER_SENSOR_NUM,
	"Invalid size of board_power_sensors");

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
static void _board_sensors_probe(struct board_sens_info * sens_info, int num_sensors,
	const char * label)
{
	int i, err, present = 0;
	struct sensor_value sens_val;

	for (i = 0; i < num_sensors; i++) {
		if (!device_is_ready(sens_info[i].dev)) {
			LOG_WRN("Board Sensor %s: device not ready, marking absent",
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
			LOG_WRN("Board Sensor %s: not responding (err %d), marking absent",
				sens_info[i].name, err);
		}
	}

	LOG_INF("Board %s sensors: %d/%d present", label, present, num_sensors);
}

static void _board_sensors_update(struct board_sens_info * sens_info, int num_sensors,
	struct hwmon_sdata * hwmon)
{
	int i, err;
	struct sensor_value sens_val;

	for (i = 0; i < num_sensors; i++) {
		if (!sens_info[i].valid)
			continue;

		err = sensor_sample_fetch(sens_info[i].dev);
		if (err) {
			LOG_ERR("Board Sensor %s sample failed: %d", sens_info[i].name, err);
			continue;
		}

		sensor_channel_get(sens_info[i].dev, sens_info[i].chan, &sens_val);
		if (err) {
			LOG_ERR("Board Sensor %s chan %d read failed: %d", sens_info[i].name,
				sens_info[i].chan, err);
			continue;
		}

		hwmon_sdata_update(&hwmon[i], &sens_val);

#if (CONFIG_BOARD_SENSOR_LOG_LEVEL >= LOG_LEVEL_DBG)
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

void board_sensors_hwmon_setting(void)
{
	int i, num_sensors;

	__ASSERT(hwmon_data != NULL, "hwmon_data is NULL");

	LOG_INF("The number of board thermal sensors is %d", BOARD_THERMAL_SENSOR_NUM);
	LOG_INF("The number of board voltage sensors is %d", BOARD_VOLTAGE_SENSOR_NUM);
	LOG_INF("The number of board current sensors is %d", BOARD_CURRENT_SENSOR_NUM);
	LOG_INF("The number of board power sensors is %d", BOARD_POWER_SENSOR_NUM);

	_board_sensors_probe(board_thermal_sensors, ARRAY_SIZE(board_thermal_sensors), "thermal");
	_board_sensors_probe(board_voltage_sensors, ARRAY_SIZE(board_voltage_sensors), "voltage");
	_board_sensors_probe(board_current_sensors, ARRAY_SIZE(board_current_sensors), "current");
	_board_sensors_probe(board_power_sensors, ARRAY_SIZE(board_power_sensors), "power");

	num_sensors = ARRAY_SIZE(board_thermal_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->board_mon_thermal[i], hwmon_temp);
		LOG_INF("BOARD SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->board_mon_thermal[i], hwmon_data),
			board_thermal_sensors[i].name,
			board_thermal_sensors[i].valid ? "" : " (absent)");
	}

	num_sensors = ARRAY_SIZE(board_voltage_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->board_mon_voltage[i], hwmon_in);

		LOG_INF("BOARD SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->board_mon_voltage[i], hwmon_data),
			board_voltage_sensors[i].name,
			board_voltage_sensors[i].valid ? "" : " (absent)");
	}

	num_sensors = ARRAY_SIZE(board_current_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->board_mon_current[i], hwmon_curr);

		LOG_INF("BOARD SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->board_mon_current[i], hwmon_data),
			board_current_sensors[i].name,
			board_current_sensors[i].valid ? "" : " (absent)");
	}

	num_sensors = ARRAY_SIZE(board_power_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->board_mon_power[i], hwmon_power);

		LOG_INF("BOARD SENS MAP: hwmon[%02ld] %s%s",
			HWMON_SRAM_ENTRY_IDX(&hwmon_data->board_mon_power[i], hwmon_data),
			board_power_sensors[i].name,
			board_power_sensors[i].valid ? "" : " (absent)");
	}
}

void board_thermal_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(board_thermal_sensors) == ARRAY_SIZE(hwmon_data->board_mon_thermal),
		"Invalid size of hwmon board_mon_thermal");

	_board_sensors_update(board_thermal_sensors, ARRAY_SIZE(board_thermal_sensors),
		hwmon_data->board_mon_thermal);
}

void board_voltage_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(board_voltage_sensors) == ARRAY_SIZE(hwmon_data->board_mon_voltage),
		"Invalid size of hwmon board_mon_voltage");

	_board_sensors_update(board_voltage_sensors, ARRAY_SIZE(board_voltage_sensors),
		hwmon_data->board_mon_voltage);
}

void board_current_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(board_current_sensors) == ARRAY_SIZE(hwmon_data->board_mon_current),
		"Invalid size of hwmon board_mon_current");

	_board_sensors_update(board_current_sensors, ARRAY_SIZE(board_current_sensors),
		hwmon_data->board_mon_current);
}

void board_power_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(board_power_sensors) == ARRAY_SIZE(hwmon_data->board_mon_power),
		"Invalid size of hwmon board_mon_power");

	_board_sensors_update(board_power_sensors, ARRAY_SIZE(board_power_sensors),
		hwmon_data->board_mon_power);
}

void board_sensors_update(void)
{
	if (hwmon_data == NULL) {
		return;
	}

	__ASSERT(ARRAY_SIZE(board_thermal_sensors) == ARRAY_SIZE(hwmon_data->board_mon_thermal),
		"Invalid size of hwmon board_mon_thermal");
	__ASSERT(ARRAY_SIZE(board_voltage_sensors) == ARRAY_SIZE(hwmon_data->board_mon_voltage),
		"Invalid size of hwmon board_mon_voltage");
	__ASSERT(ARRAY_SIZE(board_current_sensors) == ARRAY_SIZE(hwmon_data->board_mon_current),
		"Invalid size of hwmon board_mon_current");

	_board_sensors_update(board_thermal_sensors, ARRAY_SIZE(board_thermal_sensors),
		hwmon_data->board_mon_thermal);

	_board_sensors_update(board_voltage_sensors, ARRAY_SIZE(board_voltage_sensors),
		hwmon_data->board_mon_voltage);

	_board_sensors_update(board_current_sensors, ARRAY_SIZE(board_current_sensors),
		hwmon_data->board_mon_current);

	_board_sensors_update(board_power_sensors, ARRAY_SIZE(board_power_sensors),
		hwmon_data->board_mon_power);
}
