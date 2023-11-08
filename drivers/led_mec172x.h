/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LED_H__
#define __LED_H__

struct led_dev_stat {
	uint8_t rgb:1;
	uint8_t pwm:1;
	uint8_t update_color:1;
	uint8_t update_brightness:1;
	uint8_t update_blink:1;
	uint8_t :3;
};

struct led_dev {
	const struct device *dev;
	uint32_t color;
	uint32_t brightness;
	uint16_t on;
	uint16_t off;
	uint8_t owned;
	struct {
		uint8_t rgb:1;
		uint8_t pwm:1;
		uint8_t update_color:1;
		uint8_t update_brightness:1;
		uint8_t update_blink:1;
		uint8_t :3;
	};
};

/**
 * @brief  set the led color.
 *
 * @param led_idx led index.
 * @param .
 *
 * @return 0 if success, otherwise error code.
 */

int led_init(int idx, struct led_dev *tbl);

int led_color_set(uint8_t led_idx, uint8_t *color_map);

int led_brightness_set(uint8_t led_idx, uint16_t brightness);

int led_blink_set(uint8_t led_idx, uint16_t on, uint16_t off);

#endif	/* __LED_H__ */
