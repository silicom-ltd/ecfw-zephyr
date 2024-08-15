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
#include "current_monitor.h"
#include "board_config.h"

LOG_MODULE_REGISTER(currsens, CONFIG_CURRENT_MONITOR_LOG_LEVEL);
int16_t adc_amps_val[ADC_CH_TOTAL];

#define DT_DRV_COMPAT current_sense_amplifier

#define CURRENT_MONITOR(inst)				\
       DEVICE_DT_GET(DT_NODELABEL(current##inst)),	

static const struct device *current_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(CURRENT_MONITOR)
};

int current_monitor_init(uint32_t adc_channel_bits)
{
	int i;
	int num_sensors = ARRAY_SIZE(current_sensors);

	for (i = 0; i < num_sensors; i++) {
		LOG_INF("Sensor %d, name: %s\n", i, current_sensors[i]->name);
	}	

	return 0;
}


void current_monitor_update(void)
{
	int i, num_sensors = ARRAY_SIZE(current_sensors);
	struct sensor_value amps;
	int err;

	for (i = 0; i < num_sensors; i++) {
		err = sensor_sample_fetch_chan(current_sensors[i], SENSOR_CHAN_CURRENT);
		if (err) {
			LOG_WRN("ADC current reading failed %d, %s\n", i, current_sensors[i]->name);
			continue;
		}
		sensor_channel_get(current_sensors[i], SENSOR_CHAN_CURRENT, &amps);
		LOG_INF("ADC current read: %d.%d", amps.val1,amps.val2);
		adc_amps_val[i] = amps.val1;
	}
}

#undef DT_DRV_COMPAT
