/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
#include "board.h"
#include "board_config.h"
#include "smc.h"
#include "smchost.h"
#include "smchost_commands.h"
#include "scicodes.h"
#include "sci.h"
#include "acpi.h"
#include "ledmgmt.h"

LOG_MODULE_DECLARE(smchost, CONFIG_SMCHOST_LOG_LEVEL);

static void update_pwm_led(void)
{
	host_update_led(g_acpi_tbl.acpi_pwm_led_idx,
			g_acpi_tbl.acpi_pwm_led_val);
}

static void update_gpio_led(void)
{
	host_update_led(g_acpi_tbl.acpi_gpio_led_idx,
			g_acpi_tbl.acpi_gpio_led_val);
}

static void update_led_peripherals_status(void)
{
	uint8_t hw_peripherals_sts[] = {0x0, 0x0};

	/* Get led status */
	get_led_peripherals_status(hw_peripherals_sts);

	/* Send hw peripherals status to BIOS */
	send_to_host(hw_peripherals_sts, sizeof(hw_peripherals_sts));
}

void smchost_cmd_led_handler(uint8_t command)
{
	switch (command) {
	case SMCHOST_UPDATE_PWM_LED:
		update_pwm_led();
		break;
	case SMCHOST_UPDATE_GPIO_LED:
		update_gpio_led();
		break;
	case SMCHOST_GET_LED_PERIPHERALS_STS:
		update_led_peripherals_status();
		break;
	default:
		LOG_WRN("%s: command 0x%X without handler", __func__, command);
		break;
	}
}
