/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HOST_EVENT_H__
#define __HOST_EVENT_H__

#include <stdint.h>

enum host_event_type {
	HOST_EVENT_POWER_UP = 1,
	HOST_EVENT_SOFT_UP,
	HOST_EVENT_SOFT_DN,
	HOST_EVENT_REBOOT,
	HOST_EVENT_HOST_RESET,
	HOST_EVENT_BUS_RESET,
	HOST_EVENT_PWRBTN_UP,
	HOST_EVENT_PWRBTN_DN,
	HOST_EVENT_RSTBTN_PRESSED,
	HOST_EVENT_RSTBTN_RELEASED,
	HOST_EVENT_DNX_WARN,
	HOST_EVENT_MAX = HOST_EVENT_DNX_WARN,
};

int host_event_put(uint8_t event);
int host_event_get(uint8_t *buf, uint16_t buf_size, uint16_t * ret_size);
int host_event_count(void);

#endif /* __HOST_EVENT_H__ */
