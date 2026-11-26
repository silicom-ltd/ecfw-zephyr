/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HOST_EVENT_INC_H__
#define __HOST_EVENT_INC_H__

#include "host_event.h"

struct host_event_record {
	uint8_t  event;
	uint32_t ticks;
} __attribute__((__packed__));

int host_event_put(struct host_event_record *evt);

#endif /* __HOST_EVENT_INC_H__ */
