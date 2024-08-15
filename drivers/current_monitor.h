/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CURRENT_SENSOR_H__
#define __CURRENT_SENSOR_H__

extern int16_t adc_volt_val[16];

/**
 * @brief Initialize thermal sensor module.
 *
 * @param adc_channel_bits bit field value for the adc channels to be enabled.
 *
 * @return 0 if success, otherwise error code.
 */
int current_monitor_init(uint32_t adc_channel_bits);


/**
 * @brief  Read all the thermal sensors.
 *
 * This function call reads all ADC thermal sensors enabled in init, and
 * updates adc_temp_val array for respective ADC channel reads.
 */
void current_monitor_update(void);

#endif	/* __CURRENT_SENSOR_H__ */
