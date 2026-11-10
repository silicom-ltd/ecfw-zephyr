/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT ntc_thermistor
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include "therm.h" // this is in case of LUT implementation

LOG_MODULE_REGISTER(ntc_thermistor, CONFIG_SENSOR_LOG_LEVEL);

struct ntc_temp_config {
	/* Thermistor ADC channel */
	const struct adc_dt_spec chan;
	const uint16_t b_const;
};

struct ntc_temp_data {
	struct adc_sequence adc_seq;
	uint16_t sample_buffer;
	uint16_t raw;
};

int16_t adc_to_temp(uint16_t adc)
{
    uint16_t idx;
    int16_t adc0;
    int16_t adc1;
    int16_t temp0 = 0; // prevent g++ warning
    int table_size = ARRAY_SIZE(therm_table);

    if (adc > therm_table[0])
    {
	adc0 = therm_table[0];
	adc1 = therm_table[1];
        temp0 = TSTART;
    }
    else if (adc <= therm_table[table_size-1])
    {
	adc0 = therm_table[table_size - 2];
	adc1 = therm_table[table_size -1];
        temp0 = TSTOP;
    }

    else
    {
        for (idx = 0; idx < table_size; idx++)
        {
            adc0 = therm_table[idx];
            adc1 = therm_table[idx + 1];
            if (adc > adc1)
            {
                temp0 = TSTART + idx; 
                break;
            }
        }
    }

    int16_t delta = (int16_t)adc - adc0;
    int16_t range = adc1 - adc0;
    int16_t half_digit = range >> 1;

    int16_t temp = delta * 1 + half_digit;
    temp /= range;
    temp += temp0;

    return temp;
}

static int ntc_sample_fetch(const struct device *dev,
			    enum sensor_channel chan)
{
	const struct ntc_temp_config *cfg = dev->config;
	struct ntc_temp_data *data = dev->data;
	struct adc_sequence *seq = &data->adc_seq;
	int err;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP) {
		return -ENOTSUP;
	};

	err = adc_read(cfg->chan.dev, seq);
	if (!err) {
		data->raw = data->sample_buffer;
		LOG_DBG("Raw NTC data read: %d",data->raw);
	}

	return err;
}

static int ntc_temp_channel_get(const struct device *dev, enum sensor_channel chan,
				struct sensor_value *val)
{
//	const struct ntc_temp_config *config = dev->config;
	struct ntc_temp_data *data = dev->data;

	val->val1 = adc_to_temp((uint16_t)data->raw);

	return 0;
#if 0
	double temp;

	temp = (1 / (log((BIT(10U) - 1.0)
		/ data->raw - 1.0)
		/ config->b_const
		+ (1 / 298.15)))
		- 273.15;

	return sensor_value_from_double(val, temp);
#endif
}

static int ntc_temp_init(const struct device *dev)
{
	const struct ntc_temp_config *const config = dev->config;
	struct ntc_temp_data *const data = dev->data;
	struct adc_sequence *seq = &data->adc_seq;

	/*
	 * Setup ADC
	 */
	if (!device_is_ready(config->chan.dev)) {
		LOG_ERR("ADC device not ready");
		return -ENODEV;
	}

	LOG_DBG("setting up ADC channel %d",config->chan.channel_id);
	adc_channel_setup(config->chan.dev, &config->chan.channel_cfg);

	*seq = (struct adc_sequence){
		.channels = BIT(config->chan.channel_cfg.channel_id),
		.buffer = &data->sample_buffer,
		.buffer_size = sizeof(data->sample_buffer),
		.resolution = 10U,
	};

	return 0;
}

static const struct sensor_driver_api ntc_temp_api = {
//	.attr_set = ntc_temp_attr_set,
	.channel_get = ntc_temp_channel_get,
	.sample_fetch = ntc_sample_fetch,
};
#define ADC_GET(n)									\
	DEVICE_DT_GET(DT_INST_IO_CHANNELS_CTLR(n)

#define CHAN_CFG_GET(n)									\
	ADC_CHANNEL_CFG_DT(DT_IO_CHANNELS_CTLR(n))

#define B_CONST(n)									\
	DT_PROP(n, b_const)

#define NTC_TEMP_INIT(inst)								\
	static struct ntc_temp_config ntc_temp_cfg_##inst = {				\
		.chan = ADC_DT_SPEC_INST_GET(inst),					\
		.b_const = DT_INST_PROP(inst, b_const),					\
	};										\
											\
	static struct ntc_temp_data ntc_temp_data_##inst = {				\
	};										\
											\
	SENSOR_DEVICE_DT_INST_DEFINE(inst,						\
			ntc_temp_init,							\
			NULL,								\
			&ntc_temp_data_##inst,						\
			&ntc_temp_cfg_##inst,						\
			POST_KERNEL,							\
			CONFIG_SENSOR_INIT_PRIORITY,					\
			&ntc_temp_api);

DT_INST_FOREACH_STATUS_OKAY(NTC_TEMP_INIT)
