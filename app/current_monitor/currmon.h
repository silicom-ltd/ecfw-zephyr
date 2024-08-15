/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __CURRENT_MON_H__
#define __CURRENT_MON_H__

/**
 * @brief Current management task.
 *
 * - Reading current sensors over ADC
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void current_monitor_thread(void *p1, void *p2, void *p3);

#endif	/* __CURRENT_MON_H__ */
