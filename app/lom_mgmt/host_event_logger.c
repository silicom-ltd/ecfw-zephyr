/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "lom_mgmt_proc_inc.h"
#include "host_event_inc.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define MAX_PENDING_HOST_EVENTS 16
#define HOST_EVENT_THREAD_STACK_SIZE 1024

K_MSGQ_DEFINE(host_event_msgq, sizeof(struct host_event_record), MAX_PENDING_HOST_EVENTS, 1);

static K_KERNEL_STACK_DEFINE(host_event_thread_stack, HOST_EVENT_THREAD_STACK_SIZE);

#define HOST_EVENT_THREAD_PRIORITY K_PRIO_COOP(4)

static struct k_thread host_event_thread;

static atomic_t host_event_logger_started;

static void host_event_logger_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	atomic_set(&host_event_logger_started, 1);

	LOG_INF("Host Event Logger start OK");

	struct host_event_record evt;

	while (1) {
		if (k_msgq_get(&host_event_msgq, &evt, K_FOREVER) == 0) {
			host_event_put(&evt);
		}
	}
}

int host_event_submit(uint8_t event)
{
	if (!atomic_get(&host_event_logger_started)) {
		LOG_WRN("Host event logger not started, yet! event<%d> dropped", event);
		return -1;
	}

	struct host_event_record evt = { event, k_uptime_get_32() };

	if (k_msgq_put(&host_event_msgq, &evt, K_NO_WAIT) != 0) {
		LOG_ERR("Host event msgq full, event<%d> dropped!", event);
		return -1;
	}

	return 0;
}


int host_event_logger_init(void)
{
	k_thread_create(&host_event_thread, host_event_thread_stack,
		K_KERNEL_STACK_SIZEOF(host_event_thread_stack),
		host_event_logger_thread, NULL, NULL, NULL,
		HOST_EVENT_THREAD_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&host_event_thread, "LOM_MGMT_HEVTLOG");

	return 0;
}

SYS_INIT(host_event_logger_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#else

#include "host_event_inc.h"

int host_event_submit(uint8_t event)
{
	(void)event;

	return 0;
}

#endif
