/*
 * Copyright (c) 2020 Intel Corporation
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

/*
 * PWM channel uses main clock frequency i.e. 48 MHz.
 * For desired frequency of 60 kHz, division should be:
 *
 * division = main clock freq / desired freq.
 *          = 48 MHz / 60 kHz = 800.
 *
 * PWM Duty cycle = pulse width / period.
 *
 * To calculate duty cycle in percentage, multiplier should be:
 * multiplier = division / 100
 *            = 800 / 100 = 8.
 */
#define	PWM_FREQ_MULT		8

#define MAX_DUTY_CYCLE		100u

#define DT_FAN_INST(x)		DT_ALIAS(fan##x)
#define DT_TACH_INST(x)		DT_NODELABEL(tach##x)
#define PWM_LABEL(x)		DT_PROP(DT_PWM_INST(x), label)
#define TACH_LABEL(x)		DT_PROP(DT_TACH_INST(x), label)

#define	PWM_DEV_LIST_SIZE	(PWM_CH_08 + 1)
#define	TACH_DEV_LIST_SIZE	(TACH_CH_03 + 1)

static const struct device *pwm_dev[] =
{
#if DT_NODE_HAS_STATUS(DT_FAN_INST(0), okay)
	DEVICE_DT_GET(DT_FAN_INST(0)),
#endif
#if DT_NODE_HAS_STATUS(DT_FAN_INST(1), okay)
	DEVICE_DT_GET(DT_FAN_INST(1)),
#endif
};
static const struct device *tach_dev[] =
{
#if DT_NODE_HAS_STATUS(DT_TACH_INST(0), okay)
	DEVICE_DT_GET(DT_TACH_INST(0)),
#endif
#if DT_NODE_HAS_STATUS(DT_TACH_INST(1), okay)
	DEVICE_DT_GET(DT_TACH_INST(1)),
#endif
};

int fan_init(void)
{
	return ARRAY_SIZE(pwm_dev);
}

int fan_power_set(bool power_state)
{
	/* fan disable circuitry is active low. */
	return gpio_write_pin(FAN_PWR_DISABLE_N, power_state);
}

int fan_set_duty_cycle(enum fan_type fan_idx, uint8_t duty_cycle)
{
	int ret;

	if (fan_idx > ARRAY_SIZE(pwm_dev)) {
		return -ENODEV;
	}

	const struct device *pwm = pwm_dev[fan_idx];

	if (duty_cycle > MAX_DUTY_CYCLE) {
		duty_cycle = MAX_DUTY_CYCLE;
	}

	ret = pwm_set_cycles(pwm, 0, PWM_FREQ_MULT * MAX_DUTY_CYCLE,
		PWM_FREQ_MULT * duty_cycle, 0);
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

	if (fan_idx > ARRAY_SIZE(tach_dev)) {
		return -ENODEV;
	}

	const struct device *tach = tach_dev[fan_idx];
	if (!tach) {
		return -ENODEV;
	}

	ret = sensor_sample_fetch_chan(tach, SENSOR_CHAN_RPM);
	if (ret) {
		return ret;
	}

	ret = sensor_channel_get(tach, SENSOR_CHAN_RPM, &val);
	if (ret) {
		return ret;
	}

	*rpm = (uint16_t) val.val1;

	return 0;
}
