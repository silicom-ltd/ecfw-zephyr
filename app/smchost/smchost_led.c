/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
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

static void update_led_peripherals_status(void)
{
#if 0
	uint8_t hw_peripherals_sts[] = {0x0, 0x0};

	/* Get led status */
	get_led_peripherals_status(hw_peripherals_sts);

	/* Send hw peripherals status to BIOS */
	send_to_host(hw_peripherals_sts, sizeof(hw_peripherals_sts));
#endif
}

static void update_led_host_ownership(void)
{
	host_update_led_ownership(g_acpi_tbl.acpi_led_idx);
}

static void update_led_color(void)
{
	host_update_led_color(g_acpi_tbl.acpi_led_idx, g_acpi_tbl.acpi_led_val_l,
			g_acpi_tbl.acpi_led_val_h);
}

static void update_led_brightness(void)
{
	LOG_DBG("%s: called", __func__);
	host_update_led_brightness(g_acpi_tbl.acpi_led_idx, g_acpi_tbl.acpi_led_brightness[g_acpi_tbl.acpi_led_idx]);
}

static void update_led_blink(void)
{
	host_update_led_blink(g_acpi_tbl.acpi_led_idx, g_acpi_tbl.acpi_led_val_l,
			g_acpi_tbl.acpi_led_val_h);
}

void smchost_cmd_led_handler(uint8_t command)
{
	switch (command) {
#if 0
	case SMCHOST_GET_LED_PWM_OFS:
		get_led_ofs();
		break;
#endif
	case SMCHOST_UPDATE_LED_COLOR:
		update_led_color();
		break;
	case SMCHOST_UPDATE_LED_BRIGHTNESS:
		update_led_brightness();
		break;
	case SMCHOST_UPDATE_LED_BLINK:
		update_led_blink();
		break;
	case SMCHOST_GET_LED_PERIPHERALS_STS:
		update_led_peripherals_status();
		break;
	case SMCHOST_UPDATE_LED_SET_OWNER:
		update_led_host_ownership();
		break;
	default:
		LOG_WRN("%s: command 0x%X without handler", __func__, command);
		break;
	}
}
