/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>
#include "ledmgmt.h"
#include "led_mec172x.h"
#include "board_config.h"
#include "board_led.h"
#include "smc.h"
#include "sci.h"
#include "scicodes.h"
#include "smchost.h"
#include "smchost_extended.h"
#include "smchost_commands.h"
#include "pwrplane.h"
#include "memops.h"
#include "gpio_ec.h"
#include "task_handler.h"
//#include "gamma.h"

LOG_MODULE_REGISTER(ledmgmt, CONFIG_LED_MGMT_LOG_LEVEL);

static struct led_dev *led_tbl;
static uint8_t max_led_dev;
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
static struct k_sem led_lock;

void led_request()
{
	k_sem_give(&led_lock);
}
#else
static bool led_update;
#endif

#if 0
struct led_device *led_dev_tbl;
static uint8_t max_led_dev;
static uint8_t led_duty_cycle[16];
static bool led_duty_cycle_change;

void get_pwm_led_peripherals_status(uint8_t *hw_peripherals_sts)
{
	uint8_t idx = 0x0;
#if 0
	/* First byte contains led status, bits 0-3 are led index, bit 7 is
	 * led presence, 1 = present, 0 = not present.  2nd byte indicates type 
	 * (bits 3-0, bits 7-4 indicate group if RGB).
	 * 3rd byte indicates color (if 2nd byte is RGB), 4th byte indicates
	 * panel and position (bits 0-2 panel, 3-4 vertical, 5-6 horizontal).
	 * Panel:
	 * 	0 - Top
	 * 	1 - Bottom
	 * 	2 - Left
	 * 	3 - Right
	 * 	4 - Front
	 * 	5 - Back
	 * 	6 - Unknown
	 *
	 * Vertical Position:
	 * 	0 - Upper
	 * 	1 - Center
	 * 	2 - Lower
	 *
	 * Horzontal Position:
	 * 	0 - Left
	 * 	1 - Center
	 * 	2 - Right 
	 */
#endif
	/*
	 * 2 bytes contain bitmap of led pwms
	 */

	/* Update pwm led status */
	for (idx = 0; idx < max_pwm_dev; idx++) {
		if (led_dev_tbl[idx].num.pwm < 8)
			hw_peripherals_sts[0] |= BIT(led_dev_tbl[idx].num.pwm);
		else
			hw_peripherals_sts[1] |= BIT(led_dev_tbl[idx].num.pwm - 8);
	}
}
#endif

static void init_leds(void)
{
	board_led_dev_tbl_init(&max_led_dev, &led_tbl);
	LOG_INF("Found %d leds", max_led_dev);
	led_init(max_led_dev, led_tbl);

}

bool is_led_controlled_by_host(uint8_t idx)
{
	if (!is_system_in_acpi_mode()) {
		LOG_DBG("LED control is over-ridden when not in ACPI mode.");
		return 0;
	}

	if (!led_tbl[idx].owned) {
		LOG_INF("LED %d is not owned by host", idx);
		return 0;
	}
	return 1;
}

void host_update_led_ownership(uint8_t idx)
{
	if (idx < max_led_dev) {
		led_tbl[idx].owned = 1;
	}
}

void host_update_led_color(uint8_t idx, uint16_t greenblue, uint16_t red)
{
	if (!is_led_controlled_by_host(idx)) {
		LOG_INF("LED %d is not in host control", idx);
		return;
	}
	if (!led_tbl[idx].rgb) {
		LOG_INF("LED %d is not an RGB led", idx);
		return;
	}
	if (idx < max_led_dev) {
		led_tbl[idx].color = ((red & 0xFF) << 16) | greenblue;
		led_tbl[idx].update_color = 1;
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		led_update = 1;
#endif
		LOG_INF("Updating led %d color", idx);
	} else {
		LOG_WRN("Invalid led idx %d", idx);
	}
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	led_request();
#endif
}

void host_update_led_brightness(uint8_t idx, uint8_t brightness)
{
	if (!is_led_controlled_by_host(idx)) {
		LOG_INF("LED %d is not in host control", idx);
		return;
	}

	if (idx < max_led_dev) {
		led_tbl[idx].brightness = brightness;
		led_tbl[idx].update_brightness = 1;
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		led_update = 1;
#endif
		LOG_INF("Updating led %d brightness", idx);
	} else {
		LOG_WRN("Invalid led idx %d", idx);
	}
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	led_request();
#endif
}

void host_update_led_blink(uint8_t idx, uint16_t on, uint16_t off)
{
	if (!is_led_controlled_by_host(idx)) {
		LOG_INF("LED %d is not in host control", idx);
		return;
	}
	if (!led_tbl[idx].rgb && !led_tbl[idx].pwm) {
		LOG_INF("LED %d is not PWM-controlled", idx);
		return;
	}
	if (idx < max_led_dev) {
		led_tbl[idx].on = on;
		led_tbl[idx].off = off;
		led_tbl[idx].update_blink = 1;
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		led_update = 1;
#endif
		LOG_INF("Updating led %d blink rate", idx);
	} else {
		LOG_WRN("Invalid led idx %d", idx);
	}
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	led_request();
#endif
}

static void manage_leds(void)
{
	/* 
	 * ignore any attempt to control leds outside of S0
	 */
	if ((pwrseq_system_state() != SYSTEM_S0_STATE) ||
		(smchost_is_system_in_cs())) {
		return;
	}
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	if (led_update) {
		led_update = 0;
#endif
		for (uint8_t idx = 0; idx < max_led_dev; idx++) {
			if (led_tbl[idx].update_color) {
				uint8_t color[3];
				color[0] = (led_tbl[idx].color) >> 16;
				color[1] = ((led_tbl[idx].color) >> 8) & 0xFF;
				color[2] = led_tbl[idx].color & 0xFF;
				led_color_set(idx, color);
				led_tbl[idx].update_color = 0;
			}	
#if 0
			if (led_tbl[idx].update_brightness) {
				led_brightness_set(idx, led_tbl[idx].brightness);
				led_tbl[idx].update_brightness = 0;
			}
#endif
			if (led_tbl[idx].update_blink) {
				led_blink_set(idx, led_tbl[idx].on, led_tbl[idx].off);
				led_tbl[idx].update_blink = 0;
			}
		}
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	}
#endif
#if 1
	for (uint8_t idx = 0; idx < max_led_dev; idx++) {
		if (!led_tbl[idx].on || led_tbl[idx].update_brightness) { // don't change brightness of a blinker
			led_brightness_set(idx, led_tbl[idx].brightness);
			led_tbl[idx].update_brightness = 0;
			if (led_tbl[idx].on) {
				led_tbl[idx].on = 0;
				led_tbl[idx].update_blink = 1;
			}
		}
	}

#endif
}

static int color_table[] = {
#if 0
	0x9D5FB0, /* magenta-ish */
	0x318261, /* dull green */
	0x0066CC, /* a nice blue */
	0xFF66FF, /* mid pink */
	0xFFFF00, /* strong yellow */
	0x99FF99, /* very light green */
	0xF0183C, /* darkish red */
#endif
	0xFF7F00, /* amber */
};

static void manage_local_leds(void)
{
	static uint16_t level = 0;
	static uint16_t countup = 1;
	static const struct device *led_pwm_mc = DEVICE_DT_GET(DT_NODELABEL(pwmmcled0));
	int err;
	static int color_choice = 0;
	static int loops = 0;
	uint8_t colors[3];
	if (pwrseq_system_state() != SYSTEM_S0_STATE) {
		colors[0] = color_table[color_choice] >> 16;
		colors[1] = (color_table[color_choice] >> 8) & 0xFF;
		colors[2] = color_table[color_choice] & 0xFF;
	
		led_set_color(led_pwm_mc, 0, 3, colors);
		err = led_set_brightness(led_pwm_mc, 0, level);
		if (err)
			return;
		if (countup)
			level++;
		else
			level--;

		if (level == 100) {
			countup = 0;
			loops++;
		}
		else if (level == 0)
			countup = 1;
	}
	else if (!is_led_controlled_by_host(0)) {
		colors[0] = color_table[color_choice] >> 16;
		colors[1] = (color_table[color_choice] >> 8) & 0xFF;
		colors[2] = color_table[color_choice] & 0xFF;
		/* set to amber when BIOS booting */
		led_set_color(led_pwm_mc, 0, 3, colors);
		err = led_set_brightness(led_pwm_mc, 0, 100);
	}

	if (loops == 10) {
		color_choice = (color_choice + 1) % ARRAY_SIZE(color_table);
		loops = 0;
	}
}

void ledmgmt_thread(void *p1, void *p2, void *p3)
{
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	uint32_t normal_period = *(uint32_t *)p1;
#endif

	LOG_INF("LEDMGMT thread starting");
	init_leds();

#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	k_sem_init(&led_lock, 0, 1);
#endif
	while (true) {
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		k_msleep(normal_period);
		manage_local_leds();
		manage_leds();
#else
		manage_local_leds();
		k_sem_take(&led_lock, Z_TIMEOUT_MS(5));
		manage_leds();
#endif
	}
}

#ifdef CONFIG_LED_MANAGEMENT_POST
void ledmgmt_post_thread(void *p1, void *p2, void *p3)
{
	extern struct acpi_tbl g_acpi_tbl;

	uint32_t normal_period = *(uint32_t *)p1;
	static int color_choice = 0;
	static int blinking = 0;

	LOG_INF("LEDMGMT_POST thread starting");

	while (true) {
#if 0
		if (!blinking) {
			k_msleep(normal_period);
			g_acpi_tbl.acpi_led_val_l = 0;
			g_acpi_tbl.acpi_led_val_h = 0;
			smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BLINK);
		} else {
			k_msleep(normal_period*4);
			blinking++;
			blinking %= 4;
		}
#endif
		if (!blinking) {
			k_msleep(normal_period);
		if (pwrseq_system_state() != SYSTEM_S0_STATE) {
			g_acpi_tbl.acpi_led_val_l = 0;
			g_acpi_tbl.acpi_led_val_h = 0;
			smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BRIGHTNESS);
			continue;
		}
		g_acpi_tbl.acpi_led_val_l = color_table[color_choice] & 0xFFFF;
		g_acpi_tbl.acpi_led_val_h = color_table[color_choice] >> 16;
		g_acpi_tbl.acpi_led_idx = 0;

		smchost_cmd_led_handler(SMCHOST_UPDATE_LED_COLOR);
		g_acpi_tbl.acpi_led_val_l = 50;
		smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BRIGHTNESS);
		color_choice++;
		if (color_choice == 7) {
			g_acpi_tbl.acpi_led_val_l = 100;
			g_acpi_tbl.acpi_led_val_h = 100;
			smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BLINK);
			blinking = 1;
		}
		color_choice %= ARRAY_SIZE(color_table);
		} else {
			k_msleep(normal_period*4);
			blinking++;
			blinking %= 4;
			if (!blinking) {
				g_acpi_tbl.acpi_led_val_l = 1;
				g_acpi_tbl.acpi_led_val_h = 0;
				smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BLINK);
			}
		}
	}
}
#endif
