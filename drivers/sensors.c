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
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include "hwmon.h"
#include "board_config.h"

LOG_MODULE_REGISTER(thrmsens, CONFIG_THERMAL_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT murata_ncp15xh103

#define THERMAL_SENSOR(inst) \
       DEVICE_DT_GET(DT_NODELABEL(therm##inst)),	

static const struct device *ntc_thermal_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(THERMAL_SENSOR)
};

int thermal_sensors_init()
{
	int i;
	int num_sensors = ARRAY_SIZE(ntc_thermal_sensors);
	struct adc_dt_spec *adc;

	for (i = 0; i < num_sensors; i++) {
		LOG_INF("Sensor %d, name: %s\n", i, ntc_thermal_sensors[i]->name);

		adc = &ntc_thermal_sensors[i].config;

		hwmon->cfg[adc->channel_id] = adc->channel_id + THERMISTOR_TYPE 
	}	

	return 0;
}

void thermal_sensors_update(void)
{
	int i, num_sensors = ARRAY_SIZE(ntc_thermal_sensors);
	struct sensor_value temp;
	struct adc_dt_spec *adc;
	struct hmon_sdata *sdata;
	int err;

	for (i = 0; i < num_sensors; i++) {
		adc = &ntc_thermal_sensors[i].config;

		sdata = &hwmon->sdata[adc->channel_id];

		err = sensor_sample_fetch_chan(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP);
		if (err) {
			LOG_WRN("ADC Sensor reading failed %d, %s\n", i, ntc_thermal_sensors[i]->name);
			continue;
		}
		sensor_channel_get(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP, &temp);
		LOG_INF("Sensor thermistor read: %d.%dC", temp.val1, temp.val2);

		sdata->mon_in = temp.val1 << 16 | (temp.val2 / 16);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
	}
}

#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT voltage_divider

#define VOLTAGE_MONITOR(inst)				\
       DEVICE_DT_GET(DT_NODELABEL(voltage##inst)),	

static const struct device *voltage_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(VOLTAGE_MONITOR)
};

int voltage_monitor_init(void)
{
	int i;
	int num_sensors = ARRAY_SIZE(voltage_sensors);
	struct voltage_divider_dt_spec *voltage;

	for (i = 0; i < num_sensors; i++) {
		voltage = &voltage_sensors[i].config;

		LOG_INF("Sensor %d, name: %s\n", i, voltage_sensors[i]->name);
		hwmon->cfg[voltage->port.channel_id] = voltage->port.channel_id + VOLTAGE_TYPE;
	}	

	return 0;
}


void voltage_monitor_update(void)
{
	int i, num_sensors = ARRAY_SIZE(voltage_sensors);
	struct sensor_value volts;
	struct voltage_divider_dt_spec *voltage;
	struct hmon_sdata *sdata;
	int err;

	for (i = 0; i < num_sensors; i++) {
		voltage = &voltage_sensors[i].config;

		sdata = &hwmon->sdata[voltage->port.channel_id];

		err = sensor_sample_fetch_chan(voltage_sensors[i], SENSOR_CHAN_VOLTAGE);
		if (err) {
			LOG_WRN("ADC voltage reading failed %d, %s\n", i, voltage_sensors[i]->name);
			continue;
		}
		sensor_channel_get(voltage_sensors[i], SENSOR_CHAN_VOLTAGE, &volts);
		LOG_INF("ADC voltage read: %d degrees C", volts.val1);

		sdata->mon_in = (volts.val1 << 16) | (volts.val2 / 16);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
	}
}

#undef DT_DRV_COMPAT

int sensors_init()
{
	voltage_monitor_init();
	thermal_sensors_init();

	return 0;
}

void sensors_update()
{
	voltage_monitor_update();
	thermal_sensors_update();
}
