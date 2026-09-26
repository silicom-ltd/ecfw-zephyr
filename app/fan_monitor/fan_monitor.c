/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "hwmon.h"
#include "board_config.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(fanmonitor, CONFIG_FAN_MONITOR_LOG_LEVEL);

static void manage_fan_sensors(void)
{
	fan_update();
}

void fan_monitor_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

	while (true) {
		k_msleep(normal_period);

		manage_fan_sensors();
	}
}

