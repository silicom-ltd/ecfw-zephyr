/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "sensors.h"
#include "smchost.h"

LOG_MODULE_REGISTER(pecimonitor, CONFIG_PECI_MONITOR_LOG_LEVEL);

static void manage_peci_sensors(void)
{
	peci_temp_update();
}

#define CPU_PECI_CS_ACCESS_PERIOD_SEC 8U
void peci_monitor_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

	while (true) {
		/* Each thread is aware of CS
		 * Thread uses different sleep time during CS
		 * This required to enter Zephyr-LPM
		 */
		if (smchost_is_system_in_cs()) {
			k_sleep(K_SECONDS(CPU_PECI_CS_ACCESS_PERIOD_SEC));
		} else {
			k_msleep(normal_period);
		}

		manage_peci_sensors();
	}
}
