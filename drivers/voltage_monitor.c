/*
 * Copyright (c) 2024 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include "voltage_monitor.h"
#include "board_config.h"

LOG_MODULE_REGISTER(voltsens, CONFIG_VOLTAGE_MONITOR_LOG_LEVEL);
int16_t adc_volts_val[ADC_CH_TOTAL];

//#define DT_DRV_COMPAT ntc_thermistor
#define DT_DRV_COMPAT voltage_divider

#define VOLTAGE_MONITOR(inst)				\
       DEVICE_DT_GET(DT_NODELABEL(voltage##inst)),	

static const struct device *voltage_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(VOLTAGE_MONITOR)
};

int voltage_monitor_init(uint32_t adc_channel_bits)
{
	int i;
	int num_sensors = ARRAY_SIZE(voltage_sensors);

	for (i = 0; i < num_sensors; i++) {
		LOG_INF("Sensor %d, name: %s\n", i, voltage_sensors[i]->name);
	}	

	return 0;
}


void voltage_monitor_update(void)
{
	int i, num_sensors = ARRAY_SIZE(voltage_sensors);
	struct sensor_value volts;
	int err;

	for (i = 0; i < num_sensors; i++) {
		err = sensor_sample_fetch_chan(voltage_sensors[i], SENSOR_CHAN_VOLTAGE);
		if (err) {
			LOG_WRN("ADC voltage reading failed %d, %s\n", i, voltage_sensors[i]->name);
			continue;
		}
		sensor_channel_get(voltage_sensors[i], SENSOR_CHAN_VOLTAGE, &volts);
		LOG_INF("ADC voltage read: %d.%d", volts.val1,volts.val2);
		adc_volts_val[i] = volts.val1;
	}
}

#undef DT_DRV_COMPAT
