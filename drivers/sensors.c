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
#include "hwmon.h"
#include "board_config.h"

struct hwmon_sram *hwmon_data;

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

	for (i = 0; i < num_sensors; i++) {
#if 0
		LOG_INF("Sensor %d, name: %s\n", i, ntc_thermal_sensors[i]->name);
		adc = &ntc_thermal_sensors[i].config;

		hwmon->cfg[adc->channel_id] = adc->channel_id + THERMISTOR_TYPE 
#endif
	}	

	return 0;
}

void thermal_sensors_update(void)
{
	int i, num_sensors = ARRAY_SIZE(ntc_thermal_sensors);
	struct sensor_value temp;
	unsigned int temp_val;
	int multiplier;
	struct adc_dt_spec *adc_dt;
	volatile struct hwmon_sdata *sdata;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}

	for (i = 0; i < num_sensors; i++) {
		adc_dt = (struct adc_dt_spec *)ntc_thermal_sensors[i]->config;

		sdata = &hwmon_data->mon[adc_dt->channel_cfg.channel_id];

		err = sensor_sample_fetch_chan(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP);
		if (err) {
			LOG_WRN("ADC Sensor reading failed %d, %s\n", i, ntc_thermal_sensors[i]->name);
			continue;
		}
		sensor_channel_get(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP, &temp);
		LOG_INF("Sensor %d thermistor read: %d.%03dC", i, temp.val1, temp.val2);

		multiplier = 1;
		temp_val = (temp.val1 * 1000) + (temp.val2 / 1000);
		while (temp_val & 0xFFFF0000) {
			temp_val >>= 1;
			multiplier <<= 1;
		}

		sdata->mon_in = temp_val;
		if (multiplier > sdata->multiplier) {
			sdata->multiplier = multiplier;
			sdata->mon_max = temp_val;
		} else if (sdata->mon_in > sdata->mon_max) {
//		LOG_INF("\tUpdating memory @ 0x%08x with %d, mult: %d\n",(unsigned int)&sdata->mon_in, sdata->mon_in, multiplier);
			sdata->mon_max = sdata->mon_in;
		}
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (multiplier < sdata->multiplier) {
			sdata->multiplier = multiplier;
			sdata->mon_min = temp_val;
		}

		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
	}
}

#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT voltage_divider

#define VOLTAGE_MONITOR(inst)				\
       DEVICE_DT_GET(DT_NODELABEL(voltage##inst)),	

#define VOLTAGE_MONITOR_DT(inst)			\
	VOLTAGE_DIVIDER_DT_SPEC_GET(DT_NODELABEL(voltage##inst)),

static const struct device *voltage_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(VOLTAGE_MONITOR)
};

static struct voltage_divider_dt_spec data[] = {
	DT_INST_FOREACH_STATUS_OKAY(VOLTAGE_MONITOR_DT)
};

int voltage_monitor_init(void)
{
	int i;
	int num_sensors = ARRAY_SIZE(voltage_sensors);

	for (i = 0; i < num_sensors; i++) {

#if 0
		LOG_INF("Sensor %d, name: %s\n", i, voltage_sensors[i]->name);
		hwmon->cfg[voltage->port.channel_id] = voltage->port.channel_id + VOLTAGE_TYPE;
#endif
	}	

	return 0;
}


void voltage_monitor_update(void)
{
	int i, num_sensors = ARRAY_SIZE(voltage_sensors);
	struct sensor_value volts;
	struct voltage_divider_dt_spec *voltage;
	volatile struct hwmon_sdata *sdata;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}
	else {
		LOG_INF("ESPI EC1_SRAM: 0x%08x\n",(uint32_t)hwmon_data);
	}

	for (i = 0; i < num_sensors; i++) {
		voltage = &data[i];

		sdata = &hwmon_data->mon[voltage->port.channel_id];

		err = sensor_sample_fetch_chan(voltage_sensors[i], SENSOR_CHAN_VOLTAGE);
		if (err) {
			LOG_WRN("ADC voltage reading failed %d, %s\n", i, voltage_sensors[i]->name);
			continue;
		}
		sensor_channel_get(voltage_sensors[i], SENSOR_CHAN_VOLTAGE, &volts);
		LOG_INF("ADC %d voltage read: %d.%03d V", i, volts.val1, volts.val2);

		//sdata->mon_in = (volts.val1 << 16) | (volts.val2 / 16);
		sdata->mon_in = (volts.val1 * 1000) + (volts.val2 / 1000);
//		LOG_INF("\tUpdating memory @ 0x%08x with %d\n",(unsigned int)&sdata->mon_in, sdata->mon_in);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
		sdata->multiplier = 1;
	}
}

#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT current_sense_amplifier
#define CURRENT_SENSOR(inst)	\
       DEVICE_DT_GET(DT_NODELABEL(current##inst))

#define CURRENT_SENSE_DT(inst)			\
	CURRENT_SENSE_AMPLIFIER_DT_SPEC_GET(DT_NODELABEL(current##inst)),

static const struct device *current_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(CURRENT_SENSOR)
};

static struct current_sense_amplifier_dt_spec current_data[] = {
	DT_INST_FOREACH_STATUS_OKAY(CURRENT_SENSE_DT)
};

void current_sense_update(void)
{
	int i, num_sensors = ARRAY_SIZE(current_sensors);
	struct sensor_value amps;
	struct current_sense_amplifier_dt_spec *current;
	volatile struct hwmon_sdata *sdata;
	unsigned int temp_val;
	int multiplier;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}

	for (i = 0; i < num_sensors; i++) {
		current = &current_data[i];

		sdata = &hwmon_data->mon[current->port.channel_id];

		err = sensor_sample_fetch_chan(current_sensors[i], SENSOR_CHAN_CURRENT);
		if (err) {
			LOG_WRN("ADC current reading failed %d, %s\n", i, current_sensors[i]->name);
			continue;
		}
		sensor_channel_get(current_sensors[i], SENSOR_CHAN_CURRENT, &amps);
		LOG_INF("ADC %d current read: %d.%d mA", i, amps.val1, amps.val2);

		multiplier = 1;
		temp_val = amps.val1;
		while (temp_val & 0xFFFF0000) {
			temp_val >>= 1;
			multiplier <<= 1;
		}
		sdata->mon_in = temp_val;
		sdata->multiplier = multiplier;

		LOG_INF("\tUpdating memory @ 0x%08x with %d\n", (unsigned int)&sdata->mon_in, sdata->mon_in);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_min + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
		sdata->multiplier = 1;
	}
}

#undef DT_DRV_COMPAT

static const struct device *espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

struct espi_callback pltrst_cb;

static void init_hwmon_data(const struct device *dev, struct espi_callback *cb,
			    struct espi_event event)
{
	int ret;
	uint32_t hwmon_d = 0;

	if (event.evt_type == ESPI_BUS_EVENT_VWIRE_RECEIVED) {
		if ((event.evt_details == ESPI_VWIRE_SIGNAL_PLTRST)) {
			if ((event.evt_data == 1)) {
				ret = espi_read_lpc_request(espi_dev, EMI1_GET_SHARED_MEMORY, &hwmon_d);
				if (ret != 0)
					LOG_INF("Error %d returned from read_lpc_request", ret);
				hwmon_data = (struct hwmon_sram *)hwmon_d;
			}
		}
	}
}

int sensors_init()
{
	espi_init_callback(&pltrst_cb, init_hwmon_data,
			ESPI_BUS_EVENT_VWIRE_RECEIVED);
	espi_add_callback(espi_dev, &pltrst_cb);

	thermal_sensors_init();
	voltage_monitor_init();

	return 0;
}

void sensors_update()
{
	voltage_monitor_update();
	thermal_sensors_update();
	current_sense_update();
}
#undef DT_DRV_COMPAT
