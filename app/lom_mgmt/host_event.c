/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/net/net_ip.h>

#include "lom_mgmt_proc_inc.h"
#include "host_event_inc.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define HOST_EVENT_ENT_SZ sizeof(struct host_event_record)

RING_BUF_DECLARE(host_event_ring_buf,
	CONFIG_HOST_EVENT_ENTRY_MAX * HOST_EVENT_ENT_SZ);

int host_event_put(struct host_event_record *evt)
{
	struct host_event_record dummy;
	int ret;

	if (!evt) {
		return -EINVAL;
	}

	if (evt->event > HOST_EVENT_MAX) {
		LOG_ERR("Host event number out of range");
		return -EINVAL;
	}

	evt->ticks = htonl(evt->ticks);

	ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)evt, HOST_EVENT_ENT_SZ);
	if (ret < HOST_EVENT_ENT_SZ) {
		LOG_DBG_EVENT("Event ring buffer full!");

		/* evict the oldest entry */
		ret = ring_buf_get(&host_event_ring_buf, (uint8_t *)&dummy, HOST_EVENT_ENT_SZ);
		if (ret != HOST_EVENT_ENT_SZ) {
			LOG_ERR("Evict event error (%d)", ret);
			return -EIO;
		}

		/* re-enqueue the entry */
		ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)evt, HOST_EVENT_ENT_SZ);
		if (ret != HOST_EVENT_ENT_SZ) {
			LOG_ERR("Re-add event error (%d)", ret);
			return -EIO;
		}
	}

	LOG_DBG_EVENT("Add Event %d, ALL %d", evt->event,
		ring_buf_size_get(&host_event_ring_buf)/HOST_EVENT_ENT_SZ);

	avail_resource_set(AVAIL_RES_EVENT, 1);

	return 0;
}

int host_event_get(uint8_t *buf, uint16_t buf_size, uint16_t * ret_size)
{
	int event_size = ring_buf_size_get(&host_event_ring_buf);

	if (event_size == 0) {
		return 0;
	}

	uint16_t max_allowed_bytes = (buf_size / HOST_EVENT_ENT_SZ) * HOST_EVENT_ENT_SZ;

	if (event_size > max_allowed_bytes) {
		event_size = max_allowed_bytes;
		LOG_WRN("Event: destination buffer full, truncating read size");
	}

	int bytes_read = ring_buf_get(&host_event_ring_buf, buf, event_size);
	if (bytes_read < 0) {
		return -EIO;
	}

	*ret_size = (uint16_t)bytes_read;

	if (!host_event_count()) {
		avail_resource_set(AVAIL_RES_EVENT, 0);
	}

	return 0;
}

int host_event_count(void)
{
	return ring_buf_size_get(&host_event_ring_buf)/HOST_EVENT_ENT_SZ;
}
#else

#include "host_event_inc.h"

int host_event_put(struct host_event_record *evt)
{
	return 0;
}

int host_event_get(uint8_t *buf, uint16_t buf_size, uint16_t * ret_size)
{
	return 0;
}

int host_event_count(void)
{
	return 0;
}

#endif
