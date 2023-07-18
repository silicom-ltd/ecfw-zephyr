/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/led.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "led_mec172x.h"

LOG_MODULE_REGISTER(led, CONFIG_FAN_LOG_LEVEL);

static struct device led_tbl[32];

int led_blink_set(uint8_t idx, uint16_t on, uint16_t off)
{
	int ret;

	if (idx > ARRAY_SIZE(led_tbl)) {
		return -ENOTSUP;
	}

	struct device *led = &led_tbl[idx];

	if (!led) {
		return -ENODEV;
	}

	ret = led_blink(led, 0, (uint32_t)on, (uint32_t)off);

	if (ret) {
		LOG_WRN("Led blink setting error: %d", ret);
		return ret;
	}
	return 0;
}

int led_brightness_set(uint8_t idx, uint16_t brightness)
{
	int ret;

	if (idx > ARRAY_SIZE(led_tbl)) {
		return -ENOTSUP;
	}

	const struct device *led = &led_tbl[idx];

	if (!led) {
		return -ENODEV;
	}

	if (brightness == 0) {
		ret = led_off(led, 0);
		return ret;
	}

	ret = led_set_brightness(led, 0, (uint8_t)brightness);

	if (ret) {
		LOG_WRN("Led brightness  setting error: %d", ret);
		return ret;
	}
	return 0;
}

int led_color_set(uint8_t idx, uint8_t *color)
{
	int ret;

	if (idx > ARRAY_SIZE(led_tbl)) {
		return -ENOTSUP;
	}

	const struct device *led = &led_tbl[idx];

	if (!led) {
		return -ENODEV;
	}

	ret = led_set_color(led, 0, 3, color);

	if (ret) {
		LOG_WRN("Led color setting error: %d", ret);
		return ret;
	}
	return 0;
}

int led_init(int max, struct led_dev *tbl)
{

	if (max > ARRAY_SIZE(led_tbl)) {
		LOG_ERR("LED max size > led_tbl");
		return -ENOTSUP;
	}

	for (int idx = 0; idx < max; idx++) {
		led_tbl[idx] = *tbl[idx].dev;
	}

	return 0;
};
