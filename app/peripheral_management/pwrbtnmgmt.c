/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <system.h>
#include "gpio_ec.h"
#include <soc.h>
#include <zephyr/logging/log.h>
#include "sci.h"
#include "pwrplane.h"
#include "pwrbtnmgmt.h"
#include "periphmgmt.h"
#include "board_config.h"
#include "fan.h"
LOG_MODULE_REGISTER(periph, CONFIG_PERIPHERAL_LOG_LEVEL);

/* Delay to start power on sequencing - 300 ms*/
#define PWRBTN_POWERON_DELAY          300
#define MAX_PWRBTN_HANDLERS           4

struct pwrbtn_handler {
	sys_snode_t	node;
	pwrbtn_handler_t handler;
};

#if 0
/* a workq for monitoring button change */
struct btn_info {
	struct k_work work;
	uint8_t btn_index;
} pwr_btn_info;

extern void reset_btn_levels(uint8_t, int);
#endif

/* This is just a pool */
static struct pwrbtn_handler pwrbtn_handlers[MAX_PWRBTN_HANDLERS];
static int pwrbtn_handler_index;
static bool usb_pwr_btn_sts = HIGH;
static bool sys_pwr_btn_sts = HIGH;
static bool pwr_btn_out_sts = HIGH;
static bool pwrbtn_evt;
//static enum system_power_state current_state;

#if 0
void pwrbtn_input_monitor(struct k_work *item)
{
	struct btn_info *btn = 
		CONTAINER_OF(item, struct btn_info, work);

	int level;
	LOG_DBG("%s starting", __func__);

	/*
	 * monitor PWRBTN_EC_IN_N and
	 * change PM_PWRBTN output when it goes inactive
	 */
	do {
		level = gpio_read_pin(PWRBTN_EC_IN_N); /* this ought to be gotten from btn_index */
#if 1
		if (pwrseq_system_state() == SYSTEM_S5_STATE)
			break;
#endif
		k_msleep(10);
	} while (!level);

	LOG_DBG("%s finishing, PWRBTN_EC_IN_N level = %d", __func__, level);
	pwrbtn_evt = pwr_btn_out_sts = HIGH;
	gpio_write_pin(PM_PWRBTN, pwr_btn_out_sts);
	reset_btn_levels(btn->btn_index, HIGH);

	if (current_state != SYSTEM_S0_STATE) {
		LOG_DBG("%s ***** not SYSTEM_S0_STATE, triggering wake *****", __func__);
		pwrbtn_trigger_wake();
		g_pwrflags.turn_pwr_on = 1;
	}
}
#endif
void pwrbtn_btn_evt_processor(void)
{
	/* 
	 * XXX JJD, move pwrbtn_evt to file scope
	 */
	/*
	 * Calculate the executable power button status based
	 * on system and USB dock power button status.
	 */
	pwrbtn_evt = usb_pwr_btn_sts & sys_pwr_btn_sts;

	/* Current status should be different from last
	 * executed status.
	 */
	if (pwrbtn_evt != pwr_btn_out_sts) {

		pwr_btn_out_sts = pwrbtn_evt;

		LOG_DBG(" %s: power button event %d is executing ",
				__func__, pwr_btn_out_sts);

//		current_state = pwrseq_system_state();
		gpio_write_pin(PM_PWRBTN, pwr_btn_out_sts);

//		k_work_submit(&pwr_btn_info.work);
		for (int i = 0; i < MAX_PWRBTN_HANDLERS; i++) {
			if (pwrbtn_handlers[i].handler) {
				LOG_DBG("Calling handler %s", __func__);
				pwrbtn_handlers[i].handler(pwr_btn_out_sts);
			}
		}
	}
}


void sys_pwrbtn_evt_processor(uint8_t pwrbtn_evt)
{
	LOG_DBG("sys_evt=%d, usb_pwr_btn_sts=%d, sys_pwr_btn_sts=%d",
		pwrbtn_evt, usb_pwr_btn_sts, sys_pwr_btn_sts);

	sys_pwr_btn_sts = pwrbtn_evt;
	pwrbtn_btn_evt_processor();
#if 0
	if (is_system_in_acpi_mode() == 0) {
		switch (pwrbtn_evt) {
		case 0:
			g_pwrflags.turn_pwr_off = 1;
			break;
		case 1:
			g_pwrflags.turn_pwr_on = 1;
			break;
		default:
			break;
		}
	}
#endif
}

void pwrbtn_register_handler(pwrbtn_handler_t handler)
{
	LOG_DBG("%s", __func__);

	if (pwrbtn_handler_index < MAX_PWRBTN_HANDLERS - 1) {
		pwrbtn_handlers[pwrbtn_handler_index].handler = handler;
		pwrbtn_handler_index++;
	}
}

int pwrbtn_init(void)
{
	LOG_INF("%s", __func__);

//	pwr_btn_info.btn_index = 0;
//	k_work_init(&pwr_btn_info.work, pwrbtn_input_monitor);
	/* Register power button for debouncing */
	return	periph_register_button(PWRBTN_EC_IN_N,
			sys_pwrbtn_evt_processor);

}

void pwrbtn_trigger_wake(void)
{
	gpio_write_pin(PM_PWRBTN, 1);
	k_msleep(20);
	gpio_write_pin(PM_PWRBTN, 0);
	k_msleep(20);
	gpio_write_pin(PM_PWRBTN, 1);
}
