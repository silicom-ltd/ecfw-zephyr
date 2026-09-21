/*
 * Copyright (c) 2026 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __PECI_MON_H__
#define __PECI_MON_H__

/**
 * @brief PECI monitor task.
 *
 * Periodically reads CPU die temperature over PECI and updates
 * hwmon_data->peci for host visibility.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p3 pointer to additional task-specific data.
 *
 */
void peci_monitor_thread(void *p1, void *p2, void *p3);

#endif	/* __PECI_MON_H__ */
