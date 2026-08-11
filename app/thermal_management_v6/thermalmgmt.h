/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __THERMAL_MGMT_H__
#define __THERMAL_MGMT_H__

#include "fan.h"
#include "smc.h"

/* Fail safe threshold */
#define THERM_SHTDWN_THRSD			103u
/* EC tolerance range 3 deg */
#define THERM_SHTDWN_EC_TOLERANCE		3u

/**
 * @brief Thermal management task.
 *
 * This routine manages:
 * - Driving the fans from the MHO200 adaptive fan speed profile
 * - Reading the SW ambient and CPU ambient thermistors
 * - Reading the fan speed through Tach
 * - Reading the CPU temperature over PECI or PECI over eSPI channel.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void thermalmgmt_thread(void *p1, void *p2, void *p3);

/**
 * @brief Host API to allow BIOS set fan control override in Non-ACPI mode.
 *
 * By default, when system is not in ACPI mode, fan is self controlled by EC
 * based on local thermal policy. This API allows BIOS to gain CPU fan control
 * when system in non acpi mode.
 *
 * @param en 1 to enable BIOS fan override, otherwise 0.
 * @param duty_cycle fan duty cycle set by bios.
 */
void host_set_bios_fan_override(bool en, uint8_t duty_cycle);

/**
 * @brief API for SMC host to start the peci delay timer.
 *
 * PECI is prone to fail during boot to S0. Hence peci is not accessed
 * for a prescribed time duration.
 */
void peci_start_delay_timer(void);

/**
 * @brief API for SMC host to notify the CS mode exit.
 *
 */
void thermalmgmt_handle_cs_exit(void);

/**
 * @brief Clear the fan profile state.
 *
 * Restores fresh-process behaviour: the averaging window, the previous average
 * and the consecutive-drop counter are all cleared, so the next cycle takes the
 * UP branch as it would on a cold start. Called when the fans are powered down
 * on the way out of S0 so that a later run does not inherit a stale streak.
 */
void thermalmgmt_fan_profile_reset(void);

#endif	/* __THERMAL_MGMT_H__ */
