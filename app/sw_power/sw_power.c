/*
 * Copyright (c) 2025 Silicom Connectivity Solutions
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "smc.h"
#include "smchost.h"
#include "sci.h"
#include "scicodes.h"
#include "gpio_ec.h"
#include "periphmgmt.h"
#include "system.h"
#include "pwrplane.h"
#include "sw_power.h"

LOG_MODULE_REGISTER(sw_power, CONFIG_SW_POWER_SIGNAL_LOG_LEVEL);

struct sw_power_handler {
        sys_snode_t     node;
        sw_power_handler_t handler;
};

/* This is just a pool */

void sw_power_evt_processor(uint8_t sw_power_evt)
{
	int ret;

	LOG_DBG("Received SW Power event %d", sw_power_evt);
	if (sw_power_evt == 0) {
		if (pwrseq_system_state() == SYSTEM_S0_STATE) {
			ret = gpio_write_pin(SW_PWR_ON_OFF, 0);
			ret = gpio_write_pin(PM_PWRBTN, 0);
			k_busy_wait(5*1000000);
			ret = gpio_write_pin(PM_PWRBTN, 1);
	//		power_off();
		}
	}
}

void sw_power_init(void)
{
	LOG_DBG("%s", __func__);
	periph_register_button(SW_PWR_OK, sw_power_evt_processor);
}
