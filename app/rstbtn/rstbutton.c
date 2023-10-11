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
#include "smc.h"
#include "smchost.h"
#include "sci.h"
#include "scicodes.h"
#include "gpio_ec.h"
#include "periphmgmt.h"
#include "rstbutton.h"

LOG_MODULE_REGISTER(rstbutton, CONFIG_RESET_BUTTON_LOG_LEVEL);

struct rstbtn_handler {
        sys_snode_t     node;
        rstbtn_handler_t handler;
};

/* This is just a pool */
static struct rstbtn_handler rstbtn_handlers[1];
static int rstbtn_handler_index;
static bool sys_rst_btn_sts = HIGH;
static bool rst_btn_out_sts = HIGH;
static bool rstbtn_evt;

void smchost_rstbtn_handler(uint8_t rstbtn_sts)
{
	g_acpi_tbl.acpi_flags2.rst_btn = rstbtn_sts;

	if (g_acpi_state_flags.acpi_mode) {
		if (rstbtn_sts) {
			enqueue_sci(SCI_RSTBTN_RELEASE);
		} else {
			enqueue_sci(SCI_RSTBTN_PRESS);
		}
	}


}

void rstbtn_btn_evt_processor()
{
	rstbtn_evt = sys_rst_btn_sts;

	if (rstbtn_evt != rst_btn_out_sts) {
		rst_btn_out_sts = rstbtn_evt;

		for (int i = 0; i < 1; i++) {
			if (rstbtn_handlers[i].handler) {
				LOG_DBG("Calling handler %s: evt: %d", __func__, rstbtn_evt);
				rstbtn_handlers[i].handler(rst_btn_out_sts);
			}
		}

	}
}

void sys_rstbtn_evt_processor(uint8_t rstbtn_evt)
{
        sys_rst_btn_sts = rstbtn_evt;

	if ((is_system_in_acpi_mode() == 0) || (!check_btn_sci_sts(HID_BTN_SCI_RST))) {
		gpio_write_pin(SOC_RSTBTN_N, 0);
		k_msleep(20);
		gpio_write_pin(SOC_RSTBTN_N, 1);
	} else {
        	rstbtn_btn_evt_processor();
	}
}

void rstbtn_register_handler(rstbtn_handler_t handler)
{
        LOG_DBG("%s", __func__);

        if (rstbtn_handler_index < 2 - 1) {
                rstbtn_handlers[rstbtn_handler_index].handler = handler;
                rstbtn_handler_index++;
        }
}

void rstbutton_init(void)
{
	LOG_DBG("%s", __func__);
	periph_register_button(BTN_RECESSED, sys_rstbtn_evt_processor);
}
