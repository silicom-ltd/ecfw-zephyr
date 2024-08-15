/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __VOLTAGE_MON_H__
#define __VOLTAGE_MON_H__

/**
 * @brief Thermal management task.
 *
 * This routine manages:
 * - Driving the fan
 * - Reading the fan speed through Tach
 * - Reading thermal sensors over ADC
 * - Reading the CPU temperature over PECI or PECI over eSPI channel.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void voltage_monitor_thread(void *p1, void *p2, void *p3);

#endif	/* __VOLTAGE_MON_H__ */
