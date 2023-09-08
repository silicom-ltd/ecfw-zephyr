/*
 * Copyright (c) 2022 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __GPIO_MGMT_H__
#define __GPIO_MGMT_H__
/**
 * @brief GPIO management task.
 *
 * This routine manages:
 * - Keep monitoring Host controlled GPIOs and report their status
 * host memory windows.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void gpiomgmt_thread(void *p1, void *p2, void *p3);
void host_update_gpio(uint8_t idx, uint16_t value);
void host_update_gpio_status(uint8_t gpio_idx, uint8_t value);
#endif	/* __GPIO_MGMT_H__ */
