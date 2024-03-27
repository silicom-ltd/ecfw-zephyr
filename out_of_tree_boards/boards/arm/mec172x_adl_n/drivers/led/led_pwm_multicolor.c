/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT pwm_leds_multicolor

/**
 * @file
 * @brief Multicolor PWM driven LEDs
 */

#include <zephyr/drivers/led.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/math_extras.h>
#include "gamma.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(led_pwm_multicolor, CONFIG_LED_LOG_LEVEL);

struct led_pwm_mc_subled_data {
	uint32_t intensity;
	uint32_t brightness;
};

struct led_pwm_mc_subled {
	const struct pwm_dt_spec pwm;
	const uint8_t color;
};

struct led_pwm_mc {
	const int num_colors;
	const struct led_pwm_mc_subled subleds[3];
};

struct led_pwm_mc_config {
	const struct device *leds[4];
	int num_leds;
};

int led_pwm_mc_calc_components(const struct device *dev, uint8_t brightness)
{
	const struct led_pwm_mc *config = dev->config;
	struct led_pwm_mc_subled_data *data = dev->data;		
	int i;

	for (i = 0; i < config->num_colors; i++) {
		data[i].brightness = brightness *
			data[i].intensity / 255;
	}

	return 0;
}

static int led_pwm_mc_blink(const struct device *dev, uint32_t led,
			 uint32_t delay_on, uint32_t delay_off)
{
	const struct led_pwm_mc *config = dev->config;
	struct led_pwm_mc_subled_data *data = dev->data;
	const struct pwm_dt_spec *dt_pwm;
	uint32_t period_usec, pulse_usec;
	int i, err = 0;

	/*
	 * Convert delays (in ms) into PWM period and pulse (in us) and check
	 * for overflows.
	 */
	if (u32_add_overflow(delay_on, delay_off, &period_usec) ||
	    u32_mul_overflow(period_usec, 1000, &period_usec) ||
	    u32_mul_overflow(delay_on, 1000, &pulse_usec)) {
		return -EINVAL;
	}

	LOG_INF("%s called period %d pulse %d", __func__, period_usec, pulse_usec);
	for (i = 0; i < config->num_colors; i++) {
		dt_pwm = &config->subleds[i].pwm;
		if (!data[i].intensity)
			continue;
		err = pwm_set_dt(dt_pwm, PWM_USEC(period_usec), PWM_USEC(pulse_usec));
	}

	return err;
}

static int led_pwm_mc_set_brightness(const struct device *dev,
				  uint32_t led, uint8_t value)
{
	const struct led_pwm_mc *config = dev->config;
	struct led_pwm_mc_subled_data *data = dev->data;

	const struct pwm_dt_spec *dt_pwm;
	int i, err = 0;

	if (value > 255) {
		return -EINVAL;
	}

	led_pwm_mc_calc_components(dev, value);

	for (i = 0; i < config->num_colors; i++) {
		dt_pwm = &config->subleds[i].pwm;
		err = pwm_set_pulse_dt(dt_pwm, dt_pwm->period *
				data[i].brightness / 255);
	}
	return err;
}

static int led_pwm_mc_set_color(const struct device *dev, uint32_t led,
			    uint8_t num_colors, const uint8_t *color)
{
	const struct led_pwm_mc *config = dev->config;
	struct led_pwm_mc_subled_data *data = dev->data;
	int i;

	for (i = 0; i < config->num_colors; i++) {
//		data[i].intensity = gamma_table[(int)color[i]*100/255];
		data[i].intensity = gamma_table[(int)color[i]];
	}
	return 0;
}

static int led_pwm_mc_on(const struct device *dev, uint32_t led)
{
	return led_pwm_mc_set_brightness(dev, led, 100);
}

static int led_pwm_mc_off(const struct device *dev, uint32_t led)
{
	return led_pwm_mc_set_brightness(dev, led, 0);
}

#if 0
static int led_pwm_mc_init(const struct device *dev)
{
	const struct led_pwm_mc *config = dev->config;
	int j;

	for (j = 0; j < config->num_colors; j++) {
		const struct pwm_dt_spec *pwm = &config->subleds[j].pwm;

		if (!device_is_ready(pwm->dev)) {
			LOG_ERR("%s: pwm device not ready", pwm->dev->name);
			return -ENODEV;
		}
	}
	k_mutex_init(&config->lock);
	return 0;
}
#endif

#ifdef CONFIG_PM_DEVICE
static int led_pwm_mc_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	const struct led_pwm_mc_config *config = dev->config;

	/* switch all underlying PWM devices to the new state */
	for (size_t i = 0; i < config->num_colors; i++) {
		int err;
		const struct pwm_dt_spec *led = &config->leds[i].led;

		LOG_DBG("PWM %p running pm action %" PRIu32, led->dev, action);

		err = pm_device_action_run(led->dev, action);
		if (err && (err != -EALREADY)) {
			LOG_ERR("Cannot switch PWM %p power state", led->dev);
		}
	}

	return 0;
}
#endif /* CONFIG_PM_DEVICE */

static const struct led_driver_api led_pwm_mc_api = {
	.on		= led_pwm_mc_on,
	.off		= led_pwm_mc_off,
	.blink		= led_pwm_mc_blink,
	.set_brightness	= led_pwm_mc_set_brightness,
	.set_color	= led_pwm_mc_set_color,
};

#define LED_SUBLEDS_PWM_GET(node_id, prop, idx)				\
	.subleds[idx].pwm = PWM_DT_SPEC_GET_BY_IDX(node_id, idx),

#define LED_SUBLEDS_COLOR_GET(node_id, prop, idx)			\
	.subleds[idx].color = DT_PROP_BY_IDX(node_id, prop, idx),

#define LED_PWM_MC_DEVICE(n)						\
	static int led_pwm_mc_init_##n(const struct device *dev);	\
									\
	static struct led_pwm_mc_subled_data 				\
		subled_data_##n[DT_PROP_LEN(n, pwms)];			\
	static const struct led_pwm_mc mc_led_##n = {			\
		.num_colors = DT_PROP_LEN(n, pwms),			\
		DT_FOREACH_PROP_ELEM(n, pwms, LED_SUBLEDS_PWM_GET)	\
		DT_FOREACH_PROP_ELEM(n, color_mapping, LED_SUBLEDS_COLOR_GET) \
	};								\
	DEVICE_DT_DEFINE(n, led_pwm_mc_init_##n,			\
		NULL, &subled_data_##n, &mc_led_##n,			\
		POST_KERNEL, CONFIG_LED_INIT_PRIORITY,			\
		&led_pwm_mc_api);					\
									\
	static int led_pwm_mc_init_##n(const struct device *dev)	\
	{								\
		const struct led_pwm_mc *led = dev->config;		\
		int i;							\
									\
		for (i = 0; i < led->num_colors; i++) {			\
			const struct pwm_dt_spec *pwm = &led->subleds[i].pwm; \
											\
			if (!device_is_ready(pwm->dev)) {		\
				LOG_ERR("%s: pwm device not ready", pwm->dev->name);	\
				return -ENODEV;				\
			}						\
		}							\
		return 0;						\
	}

DT_FOREACH_CHILD(DT_NODELABEL(pwm_mc_leds), LED_PWM_MC_DEVICE)
