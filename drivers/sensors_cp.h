/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSORS_CP_H__
#define __SENSORS_CP_H__

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#define AMB_TEMP_SENSOR_DECLARE(node_id, prop, idx) {			\
		DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
		SENSOR_CHAN_AMBIENT_TEMP, DT_NODE_FULL_NAME(DT_PHANDLE_BY_IDX(node_id, prop, idx)) \
	},

#define DIE_TEMP_SENSOR_DECLARE(node_id, prop, idx) {			\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)), \
	SENSOR_CHAN_DIE_TEMP, DT_NODE_FULL_NAME(DT_PHANDLE_BY_IDX(node_id, prop, idx)) \
	},

#define VOLT_SENSOR_DECLARE(node_id, prop, idx) {			\
		DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
		SENSOR_CHAN_VOLTAGE, DT_NODE_FULL_NAME(DT_PHANDLE_BY_IDX(node_id, prop, idx)) \
	},

#define CURR_SENSOR_DECLARE(node_id, prop, idx) {			\
		DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
		SENSOR_CHAN_CURRENT, DT_NODE_FULL_NAME(DT_PHANDLE_BY_IDX(node_id, prop, idx)) \
	},

#define POWR_SENSOR_DECLARE(node_id, prop, idx) {			\
		DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),	\
		SENSOR_CHAN_POWER, DT_NODE_FULL_NAME(DT_PHANDLE_BY_IDX(node_id, prop, idx)) \
	},

#endif /* __SENSORS_CP_H__ */
