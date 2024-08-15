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
#include "rpmfan.h"

LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

#define FAN(inst) \
	DEVICE_DT_GET(DT_NODELABEL(rpmfan##inst)),

static const struct device rpm2pwm_fan_dev[] = {
	DT_INST_FOREACH_STATUS_OKAY(FAN)
};

#define TACH(inst) \
	DEVICE_DT_GET(DT_NODELABEL(rpmfantach##inst)),

setatic const struct device rpm2pwm_tach_dev[] = {
	DT_INST_FOREACH_STATUS_OKAY(TACH)
};

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

	if (fan_idx > ARRAY_SIZE(rpm2pwm_fan_dev)) {
		return -EINVAL;
	}

	if (rpm > UINT16_MAX) {
		rpm = UINT16_MAX;
	}

	ret = pwm_set_cycles(rpm2pwm_fan_dev[fan_idx], 0, UINT32_MAX, rpm, 0);

	if (ret) {
		LOG_WRN("Fan setting error: %d", ret);
		return ret;
	}
	return 0;
}

int fan_read_rpm(void)
{
	int ret;
	int i;
	struct sensor_value val;
	struct hmon_fdata *fdata;

	for (i = 0; i < ARRAY_SIZE(rpm2pwm_tach_dev); i++) {

		fdata = &hwmon->fan[i];
		ret = sensor_sample_fetch_chan(rpm2pwm_tach_dev[i], SENSOR_CHAN_RPM);
		if (ret) {
			return ret;
		}

		ret = sensor_channel_get(rpm2pwm_tach_dev[i], SENSOR_CHAN_RPM, &val);
		if (ret) {
			return ret;
		}

		fdata->fan_rpm = (val.val1 << 16) | (val.val2 / 16);
	}

	return 0;
}
