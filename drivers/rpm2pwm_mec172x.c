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
#include "rpmfan.h"

LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

#define DT_RPM2PWM_INST(x)	DT_NODELABEL(rpmfan##x)
#define DT_RPM2PWM_TACH_INST(x)	DT_NODELABEL(rpmfantach##x)

static const struct device *rpm2pwm_dev[2];
static const struct device *rpm2pwm_tach_dev[2];

static struct fan_dev fan_table[] = {
	{ RPM2PWM_CH_00,	RPM2PWM_TACH_CH_00	},
	{ RPM2PWM_CH_01,	RPM2PWM_TACH_CH_01	},
};

static void init_rpm2pwm_devices(void)
{
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_INST(0), okay)
	rpm2pwm_dev[RPM2PWM_CH_00] = DEVICE_DT_GET(DT_RPM2PWM_INST(0));
#endif
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_INST(1), okay)
	rpm2pwm_dev[RPM2PWM_CH_01] = DEVICE_DT_GET(DT_RPM2PWM_INST(1));
#endif
}

static void init_rpm2pwm_tach_devices(void)
{
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_TACH_INST(0), okay)
	rpm2pwm_tach_dev[RPM2PWM_TACH_CH_00] = DEVICE_DT_GET(DT_RPM2PWM_TACH_INST(0));
#endif
#if DT_NODE_HAS_STATUS(DT_RPM2PWM_TACH_INST(1), okay)
	rpm2pwm_tach_dev[RPM2PWM_TACH_CH_01] = DEVICE_DT_GET(DT_RPM2PWM_TACH_INST(1));
#endif
}

int fan_init(int size, struct fan_dev *fan_tbl)
{
	init_rpm2pwm_devices();
	init_rpm2pwm_tach_devices();

	if (size > ARRAY_SIZE(fan_table)) {
		return -ENOTSUP;
	}

	for (int idx = 0; idx < size; idx++) {

		if (!rpm2pwm_dev[fan_tbl[idx].rpm2pwm_ch]) {
			LOG_ERR("PWM ch %d not found", fan_tbl[idx].rpm2pwm_ch);
			return -ENODEV;
		}

		if (!rpm2pwm_tach_dev[fan_tbl[idx].rpm2pwm_tach_ch]) {
			LOG_ERR("Tach ch %d not found", fan_tbl[idx].rpm2pwm_tach_ch);
			return -ENODEV;
		}

		fan_table[idx].rpm2pwm_ch = fan_tbl[idx].rpm2pwm_ch;
		fan_table[idx].rpm2pwm_tach_ch = fan_tbl[idx].rpm2pwm_tach_ch;
	}

	return 0;
}

int fan_power_set(bool power_state)
{
	/* fan disable circuitry is active low. */
	return gpio_write_pin(FAN_PWR_DISABLE_N, power_state);
}

int fan_set_rpm(enum fan_type fan_idx, uint16_t rpm)
{
	int ret;

	if (fan_idx > ARRAY_SIZE(fan_table)) {
		return -ENOTSUP;
	}

	struct fan_dev *fan = &fan_table[fan_idx];

	if (fan->rpm2pwm_ch >= 2) {
		return -EINVAL;
	}

	const struct device *rpm2pwm = rpm2pwm_dev[fan->rpm2pwm_ch];

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

	if (fan_idx > ARRAY_SIZE(fan_table)) {
		return -ENOTSUP;
	}

	struct fan_dev *fan = &fan_table[fan_idx];

	if (fan->rpm2pwm_tach_ch >= 2) {
		return -EINVAL;
	}

	const struct device *rpm2pwm_tach = rpm2pwm_tach_dev[fan->rpm2pwm_tach_ch];

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
