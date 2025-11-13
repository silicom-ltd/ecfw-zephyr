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

#ifdef CONFIG_BOARD_MEC172X_ADL_N_CP
void sw_sensors_hwmon_setting(void);
void sw_thermal_sensors_update(void);
void sw_voltage_sensors_update(void);
void sw_current_sensors_update(void);
void sw_sensors_update(void);
#endif

#endif	/* __THERMAL_SENSOR_H__ */
