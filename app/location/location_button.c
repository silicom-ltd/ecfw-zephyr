/*
 * Copyright (c) 2026 Silicom Connectivity Solutions
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
#include "location_button.h"
#include <zephyr/drivers/led.h>

LOG_MODULE_REGISTER(locationbutton, CONFIG_LOCATION_BUTTON_LOG_LEVEL);

struct btn_handler {
        sys_snode_t     node;
        btn_handler_t handler;
};

/* This is just a pool */
static struct btn_handler btn_handlers[1];
static int btn_handler_index;
static bool sys_location_btn_sts = HIGH;
static bool location_btn_out_sts = HIGH;
static bool btn_evt;

const struct device *gpio_led = DEVICE_DT_GET(DT_NODELABEL(front_location));

void locationbtn_handler(uint8_t btn_sts)
{
	static int led_on = 0;

	if (btn_sts == 0) {
		if (!led_on) {
			led_blink(gpio_led, 0, 500, 500);
			led_on = 1;
			LOG_DBG("locationbtn_handler blink: %d",btn_evt);
		}
		else {
			led_on = 0;
			LOG_DBG("btn_handler off: %d",btn_evt);
			led_off(gpio_led, 0);
		}
	}

}

void locationbtn_btn_evt_processor()
{
	btn_evt = sys_location_btn_sts;
	location_btn_out_sts = btn_evt;

	for (int i = 0; i < 1; i++) {
		if (btn_handlers[i].handler) {
			LOG_DBG("Calling handler %s: evt: %d", __func__, btn_evt);
			btn_handlers[i].handler(location_btn_out_sts);
		}
	}
}

void sys_btn_evt_processor(uint8_t btn_evt)
{
        sys_location_btn_sts = btn_evt;

	locationbtn_btn_evt_processor();
}

void btn_register_handler(btn_handler_t handler)
{
        LOG_DBG("%s", __func__);

        if (btn_handler_index < 2 - 1) {
                btn_handlers[btn_handler_index].handler = handler;
                btn_handler_index++;
        }
}

void location_button_init(void)
{
	LOG_DBG("%s", __func__);
	periph_register_button(PUSHBUT_UID, sys_btn_evt_processor);
	btn_register_handler(locationbtn_handler);
}
