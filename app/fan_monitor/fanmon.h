/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FAN_MON_H__
#define __FAN_MON_H__

/**
 * @brief Fan monitor task.
 *
 * This routine manages:
 * - Reading the fan speed through Tach
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void fan_monitor_thread(void *p1, void *p2, void *p3);

#endif	/* __FAN_MON_H__ */
