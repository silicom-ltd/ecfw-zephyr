/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <errno.h>
#include <drivers/led.h>
#include <logging/log.h>
#include "board_config.h"
#include "led_mec172x.h"

LOG_MODULE_REGISTER(led, CONFIG_LED_LOG_LEVEL);

#if DT_NODE_HAS_STATUS(DT_INST(0, pwm_leds), okay)
#define LED_PWM_NODE_ID         DT_INST(0, pwm_leds)
#define LED_PWM_DEV_NAME        DEVICE_DT_NAME(LED_PWM_NODE_ID)
#define LED_PWM_LABEL(led_node_id) DT_PROP_OR(led_node_id, label, NULL),

const char *pwm_led_label[] = {
        DT_FOREACH_CHILD(LED_PWM_NODE_ID, LED_PWM_LABEL)
};
const int num_pwm_leds = ARRAY_SIZE(pwm_led_label);

#endif
#if DT_NODE_HAS_STATUS(DT_INST(0, gpio_leds), okay)
#define LED_GPIO_NODE_ID        DT_INST(0, gpio_leds)
#define LED_GPIO_DEV_NAME       DEVICE_DT_NAME(LED_GPIO_NODE_ID)
#define LED_GPIO_LABEL(led_node_id) DT_PROP_OR(led_node_id, label, NULL),

const char *gpio_led_label[] = {
        DT_FOREACH_CHILD(LED_GPIO_NODE_ID, LED_GPIO_LABEL)
};

const inst num_gpio_leds = ARRAY_SIZE(gpio_led_label);
#endif

struct led_device {
	char name[Z_DEVICE_MAX_NAME_LEN];
	const struct device *dev;
};

/*
 * PWM channel uses main clock frequency i.e. 48 MHz.
 * For desired frequency of 1 Hz, division should be:
 *
 * division = main clock freq / desired freq.
 *          = 48 MHz / 1Hz = 48000000.
 *
 * PWM Duty cycle = pulse width / period.
 *
 * To calculate duty cycle in percentage, multiplier should be:
 * multiplier = division / 100
 *            = 48000000 / 100 = 480000.
 */
#define	LED_PWM_FREQ_MULT	480000
#define MAX_DUTY_CYCLE		100u

#define DT_PWM_INST(x)		DT_NODELABEL(pwm##x)
#define PWM_LABEL(x)		DT_LABEL(DT_PWM_INST(x))

static const struct device pwm_dev[12];

/*
 * support a max of 16 LEDs
 */
static struct led_device led_table[16];

void led_set_duty_cycle(uint8_t idx, uint8_t duty_cycle)
{
	int ret;

	if (led_table[idx].type == LED_UNDEF) {
		LOG_WRN("No LED device");
		return;
	}

	if (duty_cycle > MAX_DUTY_CYCLE) {
		duty_cycle = MAX_DUTY_CYCLE;
	}

	switch(led_table[idx].type) {
		case LED_PWM:
		case LED_RGB:
			const struct device *pwm = pwm_dev[led_table[idx].num.pwm];
			ret = pwm_pin_set_cycles(pwm, 0,
				800 * MAX_DUTY_CYCLE, 800 * duty_cycle, 0);
			break;

		case LED_GPIO:
			if (duty_cycle == MAX_DUTY_CYCLE) {
				gpio_write_pin(led_table[idx].num.gpio, 1);
			} else {
				gpio_write_pin(led_table[idx].num.gpio, 0);
			}
			break;
		default:
			LOG_ERR("Unable to set led %d\n", idx);
			break;
	}

	if (ret) {
		LOG_WRN("LED error: %d", ret);
	}
}
#if 0
int led_set_duty_cycle(int led_idx, uint8_t duty_cycle)
{
	int ret;

	if (led_idx > ARRAY_SIZE(led_table)) {
		return -ENOTSUP;
	}

	struct led_dev *led = &led_table[led_idx];

	if (led->pwm_ch >= PWM_DEV_LIST_SIZE) {
		return -EINVAL;
	}

	const struct device *pwm = pwm_dev[fan->pwm_ch];

	if (!pwm) {
		return -ENODEV;
	}

	if (duty_cycle > MAX_DUTY_CYCLE) {
		duty_cycle = MAX_DUTY_CYCLE;
	}

	ret = pwm_pin_set_cycles(pwm, 0, PWM_FREQ_MULT * MAX_DUTY_CYCLE,
				PWM_FREQ_MULT * duty_cycle, 0);
	if (ret) {
		LOG_WRN("Led setting error: %d", ret);
		return ret;
	}
	return 0;
}
#endif

static void init_led_devices(void)
{
	int i;
	const struct device *led_dev;

	for (i = 0; i < num_pwm_leds; i++) {
		led_table[i].name = pwm_led_label[i]; /* memcpy? */
		led_dev = device_get_binding(pwm_led_label[i].name);
		led_table[i].dev = led_dev;
	}

	for (i = num_pwm_leds; i < num_gpio_leds; i++) {
		led_table[i].name = gpio_led_label[i]; /* memcpy */
		led_dev = device_get_binding(gpio_led_label[i].name);
		led_table[i].dev = led_dev;
	}
}

/*
 * some sort of error checking globally to ensure that PWM leds don't
 * overlap with fan PWMs
 */
void led_init(struct led_device *led_tbl)
{
	int idx = 0;

	init_led_devices();

	if (size > ARRAY_SIZE(led_table))
		return -ENOTSUP;

	while (led_tbl[idx].type != LED_UNDEF) {
		switch (led_tbl[idx].type) {
		case LED_PWM:
			if (!pwm_dev[led_tbl[idx].num.pwm]) { 
				LOG_ERR("PWM ch %d not found for led %d\n", led_tbl[idx].num.pwm, idx);
				continue;	
			}
			led_table[idx].type = LED_PWM;
			led_table[idx].num.pwm = led_tbl[idx].num.pwm;
			led_table[idx].color = led_tbl[idx].color;
			break;

		case LED_RGB:
			if (!pwm_dev[led_tbl[idx].num.pwm]) { 
				LOG_ERR("PWM ch %d not found for led %d\n", led_tbl[idx].num.pwm, idx);
				continue;	
			}
			led_table[idx].type = LED_RGB;
			led_table[idx].num.pwm = led_tbl[idx].num.pwm;
			led_table[idx].group = led_tbl[idx].group;
			led_table[idx].color = led_tbl[idx].color;
			break;
		case LED_GPIO:
			led_table[idx].type = LED_GPIO;
			led_table[idx].num.gpio = led_tbl[idx].num.gpio;
			break;
		default:
			LOG_ERR("LED type not found!\n");
			break;
		}
		idx++;
	}
}

