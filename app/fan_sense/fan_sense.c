/*
 * Copyright (c) 2025 Silicom Connectivity Solutions
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>
#include "board_config.h"
#include "smc.h"
#include "smchost.h"
#include "sci.h"
#include "scicodes.h"
#include "gpio_ec.h"
#include "periphmgmt.h"
#include "fan_sense.h"

LOG_MODULE_REGISTER(fan_sense, CONFIG_FAN_SENSE_LOG_LEVEL);

void fan1_sense_evt_processor(uint8_t sense_evt)
{
	const struct device *led = DEVICE_DT_GET(DT_NODELABEL(fan1led));

	if (sense_evt)
		(void)led_off(led, 0);
	else
		(void)led_on(led, 0);
}

void fan2_sense_evt_processor(uint8_t sense_evt)
{
	const struct device *led = DEVICE_DT_GET(DT_NODELABEL(fan2led));

	if (sense_evt)
		(void)led_off(led, 0);
	else
		(void)led_on(led, 0);
}
void fan3_sense_evt_processor(uint8_t sense_evt)
{
	const struct device *led = DEVICE_DT_GET(DT_NODELABEL(fan3led));

	if (sense_evt)
		(void)led_off(led, 0);
	else
		(void)led_on(led, 0);
}
void fan4_sense_evt_processor(uint8_t sense_evt)
{
	const struct device *led = DEVICE_DT_GET(DT_NODELABEL(fan4led));

	if (sense_evt)
		(void)led_off(led, 0);
	else
		(void)led_on(led, 0);
}
void fan_sense_init(void)
{
	LOG_DBG("%s", __func__);
	periph_register_button(FAN1_SENSE, fan1_sense_evt_processor);
	periph_register_button(FAN2_SENSE, fan2_sense_evt_processor);
	periph_register_button(FAN3_SENSE, fan3_sense_evt_processor);
	periph_register_button(FAN4_SENSE, fan4_sense_evt_processor);
}
