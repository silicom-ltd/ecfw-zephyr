/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "gpiomgmt.h"
#include "board_config.h"
#include "smchost.h"
#include "smchost_extended.h"
#include "gpio_ec.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(gpiomgmt, CONFIG_LED_MGMT_LOG_LEVEL);
extern struct gpio_ec_config mecc172x_cfg_host[11];
extern struct acpi_tbl g_acpi_tbl;

void host_update_gpio(uint8_t idx, uint16_t value)
{
	uint32_t cfg;

	if(value > 0)
		cfg = GPIO_OUTPUT_HIGH;
	else
		cfg = GPIO_OUTPUT_LOW;

	gpio_configure_pin(mecc172x_cfg_host[idx].port_pin, cfg);
}

void host_update_gpio_status(uint8_t gpio_idx, uint8_t value)
{
	if(gpio_idx <= 7) {
		if (value > 0 )
			g_acpi_tbl.usbc_mailbox_cmd |= (1 <<  gpio_idx);
		else if (value == 0)
			g_acpi_tbl.usbc_mailbox_cmd &= ~(1 <<  gpio_idx);
		else
			LOG_ERR("Wrong argument in host_update_gpio_status\n");
	}
	else if (gpio_idx <= 15) {
		if (value > 0 )
			g_acpi_tbl.usbc_mailbox_data |= (1 <<  (gpio_idx - 8));
		else if (value == 0)
			g_acpi_tbl.usbc_mailbox_data &= ~(1 <<  (gpio_idx - 8));
		else
			LOG_ERR("Wrong argument in host_update_gpio_status\n");
	}
	else {
		LOG_ERR("GPIO number out of range\n");
	}
}

static void manage_gpios(void)
{
	int level;
	static int iter;
	/*
	 * ignore any attempt to control leds outside of S0
	 */
	iter++;

	for(int i =0;i< ARRAY_SIZE(mecc172x_cfg_host); i++){
		/* Passes the enconded gpio(port_pin) to the gpio driver */
		level = gpio_read_pin(mecc172x_cfg_host[i].port_pin);
		if (level < 0) {
			LOG_ERR("Failed to read %x index=%d", gpio_get_pin(mecc172x_cfg_host[i].port_pin), i);
			return;
		}
		else if(level > 0 ) {
			host_update_gpio_status(i, 1);
		}
		else {
			host_update_gpio_status(i, 0);
		}
	}
}

void gpiomgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

	LOG_INF("GPIOMGMT thread starting");
	while (true) {
		k_msleep(normal_period);
		manage_gpios();
	}
}
