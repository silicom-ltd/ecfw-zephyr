/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SENSORS_H__
#define __SENSORS_H__

#include <zephyr/drivers/sensor.h>
#include "hwmon.h"

/**
 * @brief Initialize thermal sensor module.
 *
 * @param adc_channel_bits bit field value for the adc channels to be enabled.
 *
 * @return 0 if success, otherwise error code.
 */
int thermal_sensors_init(void);
int voltage_sensors_init(void);


/**
 * @brief  Read all the thermal sensors.
 *
 * This function call reads all ADC thermal sensors enabled in init, and
 * updates adc_temp_val array for respective ADC channel reads.
 */
void thermal_sensors_update(void);
void voltage_sensors_update(void);

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
void board_sensors_hwmon_setting(void);
void board_thermal_sensors_update(void);
void board_voltage_sensors_update(void);
void board_current_sensors_update(void);
void board_power_sensors_update(void);
void board_sensors_update(void);

/**
 * @brief  Whether board_sensors_hwmon_setting() has finished.
 *
 * board_sensors_hwmon_setting() runs on the eSPI callback thread and computes
 * each sensor's hwmon_idx after hwmon_data is already non-NULL, so a consumer
 * on another thread must check this - not just hwmon_data - before calling
 * the board_*_sensor_hwmon_idx() accessors below.
 *
 * @return true once every board sensor's hwmon_idx is valid.
 */
bool board_sensors_hwmon_ready(void);

/**
 * @brief  Look up a board sensor's slot index in hwmon_sram.
 *
 * board_sensors_hwmon_setting() records each board sensor's actual hwmon_sram
 * slot index (computed from the live struct layout) rather than a compile-time
 * constant, so callers outside this module (e.g. LOM-MGMT) that need to report
 * a board sensor's location can look it up here instead of hard-coding an
 * offset into hwmon_sram.
 *
 * @param i index into the board sensor list for that category, in devicetree
 *          declaration order (0..BOARD_*_SENSOR_NUM-1).
 *
 * @return the sensor's slot index in hwmon_sram.
 */
uint16_t board_thermal_sensor_hwmon_idx(int i);
uint16_t board_voltage_sensor_hwmon_idx(int i);
uint16_t board_current_sensor_hwmon_idx(int i);
uint16_t board_power_sensor_hwmon_idx(int i);
#endif

#endif	/* __THERMAL_SENSOR_H__ */
