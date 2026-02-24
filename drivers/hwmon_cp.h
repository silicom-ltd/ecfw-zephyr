/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_CP_H__
#define __HWMON_CP_H__

#include <zephyr/device.h>

#define SW_DIE_THERMAL_SENSOR_NUM					\
	DT_PROP_LEN_OR(DT_PATH(zephyr_user), sw_die_temp_sensors, 0)

#define SW_AMB_THERMAL_SENSOR_NUM					\
	DT_PROP_LEN_OR(DT_PATH(zephyr_user), sw_amb_temp_sensors, 0)

#define SW_VOLTAGE_SENSOR_NUM 						\
	DT_PROP_LEN_OR(DT_PATH(zephyr_user), sw_volt_sensors, 0)

#define SW_CURRENT_SENSOR_NUM						\
	DT_PROP_LEN_OR(DT_PATH(zephyr_user), sw_curr_sensors, 0)

#define SW_POWER_SENSOR_NUM						\
	DT_PROP_LEN_OR(DT_PATH(zephyr_user), sw_powr_sensors, 0)

#define SW_THERMAL_SENSOR_NUM (SW_DIE_THERMAL_SENSOR_NUM + SW_AMB_THERMAL_SENSOR_NUM)

#endif	/* __HWMON_CP_H__ */
