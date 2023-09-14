/*
 * Copyright (c) 2022 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include "board.h"
#include "board_config.h"
#include "smchost_commands.h"
#include "gpiomgmt.h"

LOG_MODULE_DECLARE(smchost, CONFIG_SMCHOST_LOG_LEVEL);
#define MAX_HOST_EC_GPIO 16

void smchost_cmd_gpio_handler(uint8_t command)
{
	switch (command) {

	case SMCHOST_UPDATE_GPIO_SET_VALUE:

		if (g_acpi_tbl.acpi_host_gpio < MAX_HOST_EC_GPIO ) {
			host_update_gpio(g_acpi_tbl.acpi_host_gpio, (g_acpi_tbl.batt_chrg_lmt >> g_acpi_tbl.acpi_host_gpio) & 0x1 );
		}
		else {
			LOG_ERR("GPIO number not supported: %d\n", g_acpi_tbl.acpi_host_gpio);
		}
		break;
	default:
		LOG_WRN("%s: command 0x%X without handler", __func__, command);
		break;
	}
}
