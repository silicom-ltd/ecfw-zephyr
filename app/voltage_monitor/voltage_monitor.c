/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
//#include "voltage_monitor.h"
//#include "voltagemon.h"
#include "hwmon.h"
#include "board_config.h"
#include "smc.h"
#include "sci.h"
#include "scicodes.h"
#include "smchost.h"
#include "memops.h"
#include "gpio_ec.h"
#include "task_handler.h"

LOG_MODULE_REGISTER(voltagemonitor, CONFIG_VOLTAGE_MONITOR_LOG_LEVEL);

static void manage_voltage_sensors(void)
{
	voltage_monitor_update();
#if 0
	for (uint8_t idx = 0; idx < max_adc_sensors; idx++) {
		smc_update_thermal_sensor(
			therm_sensor_tbl[idx].acpi_loc,
#if defined(CONFIG_BOARD_MEC172X_AZBEACH) || defined(CONFIG_BOARD_MEC172X_ADL_N)
			adc_temp_val[idx]);
#else
			adc_temp_val[therm_sensor_tbl[idx].adc_ch]);
#endif
	}

	sys_therm_sensor_trip();
#endif
}

#define CPU_VOLT_CS_ACCESS_PERIOD_SEC 8U
void voltage_monitor_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

//	voltage_monitor_init();
	
	while (true) {
		/* Each thread is aware of CS
		 * Thread uses different sleep time during CS
		 * This required to enter Zephyr-LPM
		 */
		if (smchost_is_system_in_cs()) {
			k_sleep(K_SECONDS(CPU_VOLT_CS_ACCESS_PERIOD_SEC));
		} else {
			k_msleep(normal_period);
		}

		manage_voltage_sensors();
	}
}

