/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <logging/log.h>
#include "ledmgmt.h"
#include "board_config.h"
#include "smc.h"
#include "sci.h"
#include "scicodes.h"
#include "smchost.h"
#include "memops.h"
#include "gpio_ec.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(led, CONFIG_LED_MGMT_LOG_LEVEL);

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

static void init_leds(void)
{
	int ret;
	int level;

	/* Get the list of led devices supported for the board */
	board_led_dev_tbl_init(&max_led_dev, &led_dev_tbl);
	LOG_DBG("Board has %d pwm led", max_led_dev);

	ret = led_init(led_dev_tbl);
	if (ret) {
		LOG_ERR("Failed to init fan");
	}
}

bool is_led_controlled_by_host(void)
{
	if (!is_system_in_acpi_mode()) {
		LOG_DBG("Led control is over-ridden when not in ACPI mode.");
		return 0;
	}

	return 1;
}

void host_update_led(uint8_t idx, uint8_t duty_cycle)
{
	if (!is_led_controlled_by_host()) {
		LOG_INF("Led is not in host control.");
		return;
	}

	if (idx < max_led_dev) {
		led_duty_cycle[idx] = duty_cycle;
		led_duty_cycle_change = 1;
		LOG_INF("Updating led duty cycle to %d", duty_cycle);
	} else {
		LOG_WRN("Invalid led index");
	}
}

static void manage_leds(void)
{
#if 0
	/* Disable power to fan in S5/4/3 and in CS,
	 * else continue with fan management.
	 */
	if ((pwrseq_system_state() != SYSTEM_S0_STATE) ||
		(smchost_is_system_in_cs())) {
		fan_power_set(false);
		return;
	}
#endif
	/* Enable power to leds when system is in S0 and not in CS */
//	led_power_set(true);

	if (led_duty_cycle_change) {
		led_duty_cycle_change = 0;

		for (uint8_t idx = 0; idx < max_led_dev; idx++) {
			led_set_duty_cycle(idx, led_duty_cycle[idx]);
		}
	}
}

void ledmgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;
	int err;

	init_leds();

	while (true) {
		k_msleep(normal_period);

		manage_leds();
	}
}

