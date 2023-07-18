/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include "thermal_sensor.h"
#include "board_config.h"

LOG_MODULE_REGISTER(thrmsens, CONFIG_THERMAL_SENSOR_LOG_LEVEL);
int16_t adc_temp_val[ADC_CH_TOTAL];

#define DT_DRV_COMPAT ntc_thermistor
static const struct device *ntc_thermal_sensors[] = {
	DEVICE_DT_INST_GET(0),
	DEVICE_DT_INST_GET(1),
	DEVICE_DT_INST_GET(2),
	DEVICE_DT_INST_GET(3),
};

int thermal_sensors_init(uint32_t adc_channel_bits)
{
	int i;
	int num_sensors = ARRAY_SIZE(ntc_thermal_sensors);

	for (i = 0; i < num_sensors; i++) {
		LOG_INF("Sensor %d, name: %s\n", i, ntc_thermal_sensors[i]->name);
	}	

	return 0;
}


void thermal_sensors_update(void)
{
	int i, num_sensors = ARRAY_SIZE(ntc_thermal_sensors);
	struct sensor_value temp;
	int err;

	for (i = 0; i < num_sensors; i++) {
		err = sensor_sample_fetch(ntc_thermal_sensors[i]);
		if (err) {
			LOG_WRN("ADC Sensor reading failed %d, %s\n", i, ntc_thermal_sensors[i]->name);
			continue;
		}
		sensor_channel_get(ntc_thermal_sensors[i], SENSOR_CHAN_ALL, &temp);
		LOG_INF("Sensor thermistor read: %dC", temp.val1);
		adc_temp_val[i] = temp.val1;
	}
}

