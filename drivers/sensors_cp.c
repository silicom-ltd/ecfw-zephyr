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


LOG_MODULE_REGISTER(swsens_cp, CONFIG_THERMAL_SENSOR_LOG_LEVEL);

#define SW_SENSOR_DBG

extern struct hwmon_sram *hwmon_data;

struct sw_sens_info {
	const struct device * dev;
	enum sensor_channel   chan1; /* param1: sample channel */
	enum sensor_channel   chan2; /* param2: read channel */
	char                * name;
};

#define SENSOR_DECLARE(name, chan1, chan2)			\
	DEVICE_DT_GET(DT_NODELABEL(name)), chan1, chan2, #name

static struct sw_sens_info sw_thermal_sensors[] = {
	{SENSOR_DECLARE(mfd3_temp,     SENSOR_CHAN_ALL, SENSOR_CHAN_DIE_TEMP)},
	{SENSOR_DECLARE(mfd4_temp,     SENSOR_CHAN_ALL, SENSOR_CHAN_DIE_TEMP)},
	{SENSOR_DECLARE(mfd5_temp,     SENSOR_CHAN_ALL, SENSOR_CHAN_DIE_TEMP)},
	{SENSOR_DECLARE(mfd6_temp_amb, SENSOR_CHAN_ALL, SENSOR_CHAN_DIE_TEMP)},
	{SENSOR_DECLARE(mfd6_temp_hot, SENSOR_CHAN_ALL, SENSOR_CHAN_DIE_TEMP)},
	{SENSOR_DECLARE(mfd7_temp,     SENSOR_CHAN_ALL, SENSOR_CHAN_DIE_TEMP)},
};

static struct sw_sens_info sw_voltage_sensors[] = {
	{SENSOR_DECLARE(mfd3_vin,  SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd3_vout, SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd4_vin,  SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd4_vout, SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd5_vin,  SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd5_vout, SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd7_vout, SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd8_vout, SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
	{SENSOR_DECLARE(mfd9_vout, SENSOR_CHAN_ALL, SENSOR_CHAN_VOLTAGE)},
};

static struct sw_sens_info sw_current_sensors[] = {
	{SENSOR_DECLARE(mfd3_iout, SENSOR_CHAN_ALL, SENSOR_CHAN_CURRENT)},
	{SENSOR_DECLARE(mfd4_iout, SENSOR_CHAN_ALL, SENSOR_CHAN_CURRENT)},
	{SENSOR_DECLARE(mfd5_iout, SENSOR_CHAN_ALL, SENSOR_CHAN_CURRENT)},
	{SENSOR_DECLARE(mfd6_iout, SENSOR_CHAN_ALL, SENSOR_CHAN_CURRENT)},
};


static void hwmon_sdata_update(struct hwmon_sdata *sdata, struct sensor_value *sens_value)
{
	uint32_t ret_value;
	uint16_t multiplier = 0;

	ret_value = sens_value->val1;

	while ((ret_value >> multiplier) & 0xFFFF0000)
		multiplier++;

	sdata->multiplier = multiplier;
	sdata->mon_in = ret_value >> sdata->multiplier;

	//LOG_ERR("Set hwmon_data@%p: IN %d MUL %d", sdata, sdata->mon_in, sdata->multiplier);

	if (sdata->mon_in > sdata->mon_max)
		sdata->mon_max = sdata->mon_in;
	if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
		sdata->mon_min = sdata->mon_in;
}

static void _sw_sensors_update(struct sw_sens_info * sens_info, int num_sensors,
	struct hwmon_sdata * hwmon)
{
	int i, err;
	struct sensor_value sens_val;

	for (i = 0; i < num_sensors; i++) {
		err = sensor_sample_fetch_chan(sens_info[i].dev, sens_info[i].chan1);
		if (err) {
			LOG_ERR("SW Sensor %s chan %d sample failed: %d", sens_info[i].name,
				sens_info[i].chan1, err);
			continue;
		}

		sensor_channel_get(sens_info[i].dev, sens_info[i].chan2, &sens_val);
		if (err) {
			LOG_ERR("SW Sensor %s chan %d read failed: %d", sens_info[i].name,
				sens_info[i].chan2, err);
			continue;
		}

		LOG_PRINTK(">> %13s: %d.%03d\n", sens_info[i].name,
			sens_val.val1/1000, sens_val.val1 % 1000);

		hwmon_sdata_update(&hwmon[i], &sens_val);
	}
}

void sw_sensors_hwmon_setting(void)
{
	int i, num_sensors;

	__ASSERT(hwmon_data != NULL, "hwmon_data is NULL");

	num_sensors = ARRAY_SIZE(sw_thermal_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_thermal[i], hwmon_temp);
	}

	num_sensors = ARRAY_SIZE(sw_voltage_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_voltage[i], hwmon_in);
	}

	num_sensors = ARRAY_SIZE(sw_current_sensors);
	for (i = 0; i < num_sensors; i++) {
		SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data,
			&hwmon_data->sw_mon_current[i], hwmon_curr);
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
}
