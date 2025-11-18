/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief APIs for POST code management
 */

#ifndef __POSTCODE_MGMT_H__
#define __POSTCODE_MGMT_H__

#define POSTCODE_PORT80    0
#define POSTCODE_PORT81    1

#ifdef CONFIG_POSTCODE_MONITOR
typedef void (*postcode_disp_event_handler_t)(uint16_t code);
#endif

/**
 * @brief BIOS debug port debug management.
 *
 * This routine performs management BIOS Port80 debug.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 */
void postcode_thread(void *p1, void *p2, void *p3);

/**
 * @brief Report error code reported in BIOS debug port.
 *
 * @param errcode the board error code identifier.
 */
void update_error(uint8_t errcode);


#ifdef CONFIG_POSTCODE_MONITOR
/**
 * @brief Add a post-code display event handler
 *
 * @param handler for monitoring the post-code changes.
 */

int postcode_add_disp_event_handler(postcode_disp_event_handler_t handler);
#endif

#endif /* __POSTCODE_MGMT_H__ */
