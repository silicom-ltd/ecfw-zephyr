/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RPMFAN_H__
#define __RPMFAN_H__

enum rpm2pwm_ch_num {
	RPM2PWM_CH_00,
	RPM2PWM_CH_01,
	RPM2PWM_CH_TOTAL,
	RPM2PWM_CH_UNDEF = 0xFF,
};

enum rpm2pwm_tach_ch_num {
	RPM2PWM_TACH_CH_00,
	RPM2PWM_TACH_CH_01,
	RPM2PWM_TACH_CH_TOTAL,
	RPM2WPM_TACH_CH_UNDEF = 0xFF,
};

enum fan_type {
	FAN_LEFT,
	FAN_RIGHT,
	FAN_DEV_TOTAL,
	FAND_DEV_UNDEF = 0xFF,
};

struct fan_dev {
	enum rpm2pwm_ch_num rpm2pwm_ch;
	enum rpm2pwm_tach_ch_num rpm2pwm_tach_ch;
};

/**
 * @brief Perform the the fan initialization.
 *
 * Initialize and configure below for each fan device instance:
 * - PWM channel to drive the fan.
 * - Tach channel to read the fan speed.
 *
 * @param size size of table for number of supported fan devices.
 * @param fan_tbl pointer to array of fan_dev instances.
 *
 * @return 0 if success, otherwise error code.
 */
int fan_init(int size, struct fan_dev *fan_tbl);

/**
 * @brief Set fan supply power on / off.
 *
 * Call this function when fan power supply needs to be removed like in
 * connected standby low power mode.
 *
 * @param power_state 1 - to set fan power on, else 0.
 *
 * @return 0 if success, otherwise error code.
 */
int fan_power_set(bool power_state);

/**
 * @brief  Set the fan rpmpwm channel rpm.
 *
 * @param fan_idx fan type.
 * @param rpm Fan rpm can be 0 to max fan rpm.
 *
 * @return 0 if success, otherwise error code.
 */
int fan_set_rpm(enum fan_type fan_idx, uint16_t rpm);

/**
 * @brief  read fan rpm value using tach.
 *
 * The updated fan speed i.e. rotation per minute (rpm) value is stored in the
 * fan_dev struct parameter fan->rpm.
 *
 * @param fan_idx fan type.
 * @param rpm pointer to update rpm value.
 *
 * @return 0 if success, otherwise error code.
 */
int fan_read_rpm(enum fan_type fan_idx, uint16_t *rpm);


#endif	/* __RPMFAN_H__ */
