/*
 * Copyright (c) 2023 Silicom Connectivity Solutions
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include "board_config.h"
#include "gpio_ec.h"
#include "periphmgmt.h"
#include "rstbutton.h"

LOG_MODULE_REGISTER(rstbutton, CONFIG_RESET_BUTTON_LOG_LEVEL);

static void rstbutton_handler(uint8_t btn_pressed)
{
	LOG_DBG("%s rstbtn: %d", __func__, btn_pressed);

	gpio_write_pin(SOC_RSTBTN_N, btn_pressed);
}

void rstbutton_init(void)
{
	LOG_DBG("%s", __func__);
	periph_register_button(BTN_RECESSED, rstbutton_handler);
}
