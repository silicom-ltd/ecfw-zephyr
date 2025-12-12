/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_CP_H__
#define __HWMON_CP_H__

#include <zephyr/device.h>

#if !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), sw_die_temp_sensors)
#warn "Property sw_die_temp_sensors does not exist in zephyr,user node!"
#define SW_DIE_THERMAL_SENSOR_NUM 0
#else
#define SW_DIE_THERMAL_SENSOR_NUM				\
	DT_PROP_LEN(DT_PATH(zephyr_user), sw_die_temp_sensors)
#endif

#if !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), sw_amb_temp_sensors)
#warn "Property sw_amb_temp_sensors does not exist in zephyr,user node!"
#define SW_AMB_THERMAL_SENSOR_NUM 0
	#else
#define SW_AMB_THERMAL_SENSOR_NUM 				\
	DT_PROP_LEN(DT_PATH(zephyr_user), sw_amb_temp_sensors)
#endif

#if !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), sw_volt_sensors)
#warn "Property sw_volt_sensors does not exist in zephyr,user node!"
#define SW_VOLTAGE_SENSOR_NUM 0
#else
#define SW_VOLTAGE_SENSOR_NUM (						\
	DT_PROP_LEN(DT_PATH(zephyr_user), sw_volt_sensors))
#endif

#if !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), sw_curr_sensors)
#warn "Property sw_curr_sensors does not exist in zephyr,user node!"
#define SW_CURRENT_SENSOR_NUM 0
#else
#define SW_CURRENT_SENSOR_NUM (					\
	DT_PROP_LEN(DT_PATH(zephyr_user), sw_curr_sensors))
#endif

#if !DT_NODE_HAS_PROP(DT_PATH(zephyr_user), sw_powr_sensors)
#warn "Property sw_powr_sensors does not exist in zephyr,user node!"
#define SW_POWER_SENSOR_NUM 0
#else
#define SW_POWER_SENSOR_NUM (					\
	DT_PROP_LEN(DT_PATH(zephyr_user), sw_powr_sensors))
#endif

#define SW_THERMAL_SENSOR_NUM (SW_DIE_THERMAL_SENSOR_NUM + SW_AMB_THERMAL_SENSOR_NUM)

#endif	/* __HWMON_CP_H__ */
