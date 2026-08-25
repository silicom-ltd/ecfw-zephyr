/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_CP_H__
#define __HWMON_CP_H__

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

/*
 * Add-on card overlays (snippets/mho-150, snippets/mho-200) each instantiate
 * one "silicom,board-sensors" node, pointing its phandle lists at whichever
 * PMBus/LM75/CPLD sensor devices that particular card wires up. See
 * out_of_tree_boards/boards/arm/mec172x_adl_n_cp/dts/bindings/sensor/silicom,board-sensors.yaml
 */
#define BOARD_SENSORS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(silicom_board_sensors)

#define BOARD_DIE_THERMAL_SENSOR_NUM					\
	DT_PROP_LEN_OR(BOARD_SENSORS_NODE, board_die_temp_sensors, 0)

#define BOARD_AMB_THERMAL_SENSOR_NUM					\
	DT_PROP_LEN_OR(BOARD_SENSORS_NODE, board_amb_temp_sensors, 0)

#define BOARD_VOLTAGE_SENSOR_NUM 					\
	DT_PROP_LEN_OR(BOARD_SENSORS_NODE, board_volt_sensors, 0)

#define BOARD_CURRENT_SENSOR_NUM					\
	DT_PROP_LEN_OR(BOARD_SENSORS_NODE, board_curr_sensors, 0)

#define BOARD_POWER_SENSOR_NUM						\
	DT_PROP_LEN_OR(BOARD_SENSORS_NODE, board_powr_sensors, 0)

#define BOARD_THERMAL_SENSOR_NUM (BOARD_DIE_THERMAL_SENSOR_NUM + BOARD_AMB_THERMAL_SENSOR_NUM)

#endif	/* __HWMON_CP_H__ */
