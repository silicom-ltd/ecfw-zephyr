/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EMC230X_FAN_H__
#define __EMC230X_FAN_H__

/*
 * enum pwm_ch_num, enum tach_ch_num, enum fan_type, struct fan_dev, and the
 * fan_init()/fan_power_set()/fan_set_duty_cycle()/fan_read_rpm()/fan_update()
 * declarations are shared with the MEC1501 fan_mec15xx.c implementation -
 * see fan.h. Only the EMC230x-specific additions below are declared here.
 */
#include "fan.h"

void fans_spin_down(void);

void fans_set_default(void);

/**
 * @brief  Number of EMC230x fan channels actually wired up via devicetree.
 *
 * hwmon_sram's emc230x_fan[] array is sized for the max the SoC/EMC230x pair
 * support; only this many of its slots correspond to a real fan device.
 */
int fan_count(void);

/**
 * @brief  Look up a fan's slot index in hwmon_sram.
 *
 * Mirrors board_thermal_sensor_hwmon_idx() and friends in sensors.h: the
 * offset is computed from the live hwmon_sram layout rather than assumed by
 * the caller, so LOM-MGMT can report it without hard-coding hwmon_sram.
 *
 * @param fan_idx index of the fan, 0..fan_count()-1.
 *
 * @return the fan's slot index in hwmon_sram.
 */
uint16_t fan_hwmon_idx(int fan_idx);

#endif	/* __EMC230X_FAN_H__ */
