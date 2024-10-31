/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2023-2024 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "hwmon.h"
#include "gpio_ec.h"
#include "board_config.h"
//#include "rpmfan.h"
#include "fan.h"

LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

extern struct hwmon_sram *hwmon_data;

#define MAX_DUTY_CYCLE		100u

#define DT_DRV_COMPAT	microchip_xec_rpm2pwm	
#define DT_FAN_INST(x)		DEVICE_DT_GET(DT_ALIAS(fan##x)),

static const struct device *rpm2pwm_fan_dev[] = {
	DT_INST_FOREACH_STATUS_OKAY(DT_FAN_INST)
};

#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT	microchip_xec_rpm2pwm_tach
#define DT_TACH_INST(x)		DEVICE_DT_GET(DT_ALIAS(tach##x)),

static const struct device *rpm2pwm_tach_dev[] = {
	DT_INST_FOREACH_STATUS_OKAY(DT_TACH_INST)
};

int fan_init(void)
{
	LOG_WRN("Rpm2pwm_fan_dev size %d", ARRAY_SIZE(rpm2pwm_fan_dev));	
	return ARRAY_SIZE(rpm2pwm_fan_dev);
}

int fan_power_set(bool power_state)
{
	/* fan disable circuitry is active low. */
	return gpio_write_pin(FAN_PWR_DISABLE_N, power_state);
}

int fan_set_duty_cycle(enum fan_type fan_idx, uint8_t rpm)
{
	int ret;
	struct hwmon_pdata *pdata;

	if (fan_idx > ARRAY_SIZE(rpm2pwm_fan_dev)) {
		return -EINVAL;
	}

	if (rpm > MAX_DUTY_CYCLE) {
		rpm = MAX_DUTY_CYCLE;
	}

	const struct device *pwm = rpm2pwm_fan_dev[fan_idx];

		LOG_WRN("Fan %d setting duty cycle %d", fan_idx, rpm);
	ret = pwm_set_cycles(pwm, 0, UINT32_MAX, rpm, 0);

	if (ret) {
		LOG_WRN("Fan setting error: %d", ret);
		return ret;
	}

	if (hwmon_data == NULL)
		return 0;

	pdata = &hwmon_data->pwm[fan_idx];
	pdata->pwm_in = rpm;	

	return 0;
}

int fan_read_rpm(enum fan_type fan_idx, uint16_t *rpm)
{
	struct hwmon_fdata *fdata;
       
	if (fan_idx > ARRAY_SIZE(rpm2pwm_fan_dev))
		return -ENODEV;

	fdata = &hwmon_data->fan[fan_idx];

	*rpm = fdata->fan_rpm;

	return 0;
}

int fan_update(void)
{
	int ret;
	int i;
	struct sensor_value val;
	struct hwmon_fdata *fdata;

	for (i = 0; i < ARRAY_SIZE(rpm2pwm_tach_dev); i++) {

		ret = sensor_sample_fetch_chan(rpm2pwm_tach_dev[i], SENSOR_CHAN_RPM);
		if (ret) {
			return ret;
		}

		ret = sensor_channel_get(rpm2pwm_tach_dev[i], SENSOR_CHAN_RPM, &val);
		if (ret) {
			return ret;
		}

		if (hwmon_data == NULL)
			return 0;

		fdata = &hwmon_data->fan[i];
		fdata->fan_rpm = val.val1;
	}

	return 0;
}
