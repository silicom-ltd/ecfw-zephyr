/*
 * Copyright (c) 2022 Silicom Connectivity Solutions
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <soc.h>
#include <logging/log.h>
#include "board_config.h"
#include "gpio_ec.h"
#include "periphmgmt.h"
#include "vrtt.h"

LOG_MODULE_REGISTER(vrtt_test, CONFIG_PWRMGT_LOG_LEVEL);

/* for VRTT testing, we pretend that SLP_Sx is asserted (low) */
static bool slp_pins_state = true;

void vrtt_test_handler(uint8_t vrtt_pressed)
{
	LOG_DBG("%s soc_gpio: %d", __func__, vrtt_pressed);

	slp_pins_state = !slp_pins_state;

	/* VRTT testing will have the recessed button change the state 
	 * of the SLP_Sx signals
	 */
	if (slp_pins_state == false) {
		gpio_write_pin(SLP_S3_N, 1);
		gpio_write_pin(SLP_S4_N, 1);
	} else {
		gpio_write_pin(SLP_S4_N, 0);
		gpio_write_pin(SLP_S3_N, 0);
	}
}

void vrtt_test_init(void)
{
	int pin = gpio_read_pin(PM_SLP_SUS);

	LOG_DBG("%s", __func__);

	/* if SLP_SUS_N is 0 and we got here, then monitor for VRTT testing */
	if (pin == 0) {
		/* EC controls SLP_Sx for testing */
		gpio_force_configure_pin(SLP_S3_N, GPIO_OUTPUT_LOW);
		gpio_force_configure_pin(SLP_S4_N, GPIO_OUTPUT_LOW);
		periph_register_button(BTN_RECESSED, vrtt_test_handler);
	}
}
