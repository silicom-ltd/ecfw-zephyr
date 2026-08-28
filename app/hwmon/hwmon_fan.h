/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_FAN_H__
#define __HWMON_FAN_H__

/*
 * enum pwm_ch_num, enum tach_ch_num, enum fan_type, struct fan_dev, and the
 * fan_init()/fan_power_set()/fan_set_duty_cycle()/fan_read_rpm()/fan_update()
 * declarations are shared with the MEC1501 fan_mec15xx.c implementation -
 * see fan.h. Only the additions below, specific to the generic
 * Zephyr-fan-class implementation in hwmon_fan.c, are declared here.
 */
#include "fan.h"

void fans_spin_down(void);

void fans_set_default(void);

/**
 * @brief  Tag each board_fan[] slot's hwmon class in hwmon_data->rsvd[].
 *
 * Call once hwmon_data is available, before board_fan[] is read/written.
 */
void fan_hwmon_setting(void);

/**
 * @brief  Number of fan channels actually wired up via devicetree.
 *
 * hwmon_sram's board_fan[] array is sized for the max any supported fan
 * controller needs; only this many of its slots correspond to a real fan
 * device (see the "silicom,board-sensors" node's fan-devices property).
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

#endif	/* __HWMON_FAN_H__ */
