/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LED_MEC172X_H__
#define __LED_MEC172X_H__

#define ONLY_ON		100u
#define ONLY_OFF	0u

enum led_type {
	LED_RGB,
	LED_PWM,
	LED_GPIO,
	LED_BLINK,
	LED_BREATH,
	LED_UNDEF = 0xFF
};

enum led_color {
	LED_RED,
	LED_GREEN,
	LED_BLUE,
	LED_NONE = 0xFF
};

union dev_num {
	uint32_t gpio;          // this is so we can use the gpio macros
	uint8_t pwm;
	uint8_t blink;
	uint8_t breath;
};

struct led_device {
	enum led_type type;
	union dev_num num;
	uint8_t group;  // for RGB leds
	enum led_color color;
};


/**
 * @brief Make the LED blink at the prescribed "duty cycle" percentage.
 *
 * @param idx Number representing the LED device.
 * @param duty_cycle pwm duty_cycle in % value can be between 0 to 100.
 */
int set_led_blink(int idx, uint8_t duty_cycle);

/*
 * @brief Turn on the LED
 *
 * @param idx Number representing the LED device.
 */
int set_led_on(int idx);

/**
 * @brief Turn off the LED
 *
 * @param idx Number representing the LED device.
 */
int set_led_off(int idx);

/**
 * @brief Init the LEDs
 */
void led_init(int max, struct led_device *led_table);

#endif	/* __LED_MEC172X_H__ */
