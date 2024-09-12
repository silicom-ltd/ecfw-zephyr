/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "fan.h"

LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

#define	PWM_FREQ_MULT		8

#define MAX_DUTY_CYCLE		100u
#define DT_RPM2PWM_INST(x)	DT_NODELABEL(rpmfan##x)
#define DT_RPM2PWM_TACH_INST(x)	DT_NODELABEL(rpmfantach##x)

static const struct device *rpm2pwm_dev[] = {
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_INST(0), okay)
	DEVICE_DT_GET(DT_RPM2PWM_INST(0)),
#endif
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_INST(1), okay)
	DEVICE_DT_GET(DT_RPM2PWM_INST(1)),
#endif
};
static const struct device *rpm2pwm_tach_dev[] = {
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_TACH_INST(0), okay)
	DEVICE_DT_GET(DT_RPM2PWM_TACH_INST(0)),
#endif
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_TACH_INST(1), okay)
	DEVICE_DT_GET(DT_RPM2PWM_TACH_INST(1)),
#endif
};

int fan_init(void)
{
	return ARRAY_SIZE(rpm2pwm_dev);
}

int fan_power_set(bool power_state)
{
	/* fan disable circuitry is active low. */
	return gpio_write_pin(FAN_PWR_DISABLE_N, power_state);
}

int fan_set_duty_cycle(enum fan_type fan_idx, uint8_t duty_cycle)
{
	int ret;

	if (fan_idx > ARRAY_SIZE(rpm2pwm_dev)) {
		return -ENODEV;
	}

	const struct device *rpm2pwm = rpm2pwm_dev[fan_idx];

	if (duty_cycle > MAX_DUTY_CYCLE) {
		duty_cycle = MAX_DUTY_CYCLE;
	}

	ret = pwm_set_cycles(rpm2pwm, 0, PWM_FREQ_MULT * MAX_DUTY_CYCLE,
		duty_cycle, 0);
	if (ret) {
		LOG_WRN("Fan setting error: %d", ret);
		return ret;
	}
	return 0;
}

int fan_set_rpm(enum fan_type fan_idx, uint16_t rpm)
{
	int ret;

	const struct device *rpm2pwm = rpm2pwm_dev[fan_idx];

	if (!rpm2pwm) {
		return -ENODEV;
	}

	if (rpm > UINT16_MAX) {
		rpm = UINT16_MAX;
	}

	ret = pwm_set_cycles(rpm2pwm, 0, UINT32_MAX, rpm, 0);

	if (ret) {
		LOG_WRN("Fan setting error: %d", ret);
		return ret;
	}
	return 0;
}

int fan_read_rpm(enum fan_type fan_idx, uint16_t *rpm)
{
	int ret;
	struct sensor_value val;

	const struct device *rpm2pwm_tach = rpm2pwm_tach_dev[fan_idx];

	if (!rpm2pwm_tach) {
		return -ENODEV;
	}

	ret = sensor_sample_fetch_chan(rpm2pwm_tach, SENSOR_CHAN_RPM);
	if (ret) {
		return ret;
	}

	ret = sensor_channel_get(rpm2pwm_tach, SENSOR_CHAN_RPM, &val);
	if (ret) {
		return ret;
	}

	*rpm = (uint16_t) val.val1;

	return 0;
}
