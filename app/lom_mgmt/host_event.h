/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HOST_EVENT_H__
#define __HOST_EVENT_H__

#include <stdint.h>

enum host_event_type {
	HOST_EVENT_POWER_UP = 1,   /* 1 */
	HOST_EVENT_SOFT_UP,        /* 2 */
	HOST_EVENT_SOFT_DN,        /* 3 */
	HOST_EVENT_REBOOT,         /* 4 */
	HOST_EVENT_HOST_RESET,     /* 5 */
	HOST_EVENT_RSVD1,          /* 6 */
	HOST_EVENT_RSVD2,          /* 7 */
	HOST_EVENT_PWRBTN_PRESSED, /* 8 */
	HOST_EVENT_RSTBTN_PRESSED, /* 9 */
	HOST_EVENT_RSVD3,          /* 10 */
	HOST_EVENT_DNX_WARN,       /* 11 */
	HOST_EVENT_IERR,           /* 12 */
	HOST_EVENT_MCERR,          /* 13 */
	HOST_EVENT_THERMTRIP,      /* 14 */
	HOST_EVENT_MAX = HOST_EVENT_THERMTRIP,
};

int host_event_submit(uint8_t event);
int host_event_get(uint8_t *buf, uint16_t buf_size, uint16_t * ret_size);
int host_event_count(void);

#endif /* __HOST_EVENT_H__ */
