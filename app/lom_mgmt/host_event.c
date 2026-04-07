
#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/net/net_ip.h>

#include "lom_mgmt_proc_inc.h"
#include "host_event.h"

LOG_MODULE_REGISTER(host_event, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

struct host_event_record_ent {
	uint8_t  event;
	uint32_t ticks;
} __attribute__((__packed__));

#define HOST_EVENT_ENT_SZ sizeof(struct host_event_record_ent)

RING_BUF_DECLARE(host_event_ring_buf,
	CONFIG_HOST_EVENT_ENTRY_MAX * HOST_EVENT_ENT_SZ);

int host_event_put(uint8_t event)
{
#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT
	struct host_event_record_ent e, dummy;
	int ret;

	if (event > HOST_EVENT_MAX) {
		LOG_ERR("Host event number out of range");
		return -1;
	}

	uint32_t ts = k_uptime_get_32();

	e.event = event;
	e.ticks = htonl(ts);

	ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, HOST_EVENT_ENT_SZ);
	if (ret < HOST_EVENT_ENT_SZ) {
		LOG_DBG_EVENT("Event ring buffer full!");

		/* evict the oldest entry */
		ret = ring_buf_get(&host_event_ring_buf, (uint8_t *)&dummy, HOST_EVENT_ENT_SZ);
		if (ret != HOST_EVENT_ENT_SZ) {
			LOG_ERR("Evict event error (%d)", ret);
			return -EIO;
		}

		/* re-enqueue the entry */
		ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, HOST_EVENT_ENT_SZ);
		if (ret != HOST_EVENT_ENT_SZ) {
			LOG_ERR("Re-add event error (%d)", ret);
			return -EIO;
		}
	}

	LOG_DBG_EVENT("Add Event %d, ALL %d", e.event,
		ring_buf_size_get(&host_event_ring_buf)/HOST_EVENT_ENT_SZ);

	avail_resource_set(AVAIL_RES_EVENT, 1);
#endif

	return 0;
}

int host_event_get(uint8_t *buf, uint16_t buf_size, uint16_t * ret_size)
{
#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT
	int ret;
	int off = 0;
	int event_size = ring_buf_size_get(&host_event_ring_buf);

	if (event_size == 0) {
		return 0;
	}

	while ((ret = ring_buf_get(&host_event_ring_buf, &buf[off], HOST_EVENT_ENT_SZ)) != 0) {
		if (ret != HOST_EVENT_ENT_SZ) {
			return -EIO;
		}

		off += HOST_EVENT_ENT_SZ;

		/* has space for next entry? */
		if (off + HOST_EVENT_ENT_SZ > buf_size) {
			LOG_DBG_EVENT("Event: res buff full quit");
			break;
		}
	}

	*ret_size = off;

	if (!host_event_count()) {
		avail_resource_set(AVAIL_RES_EVENT, 0);
	}
#endif

	return 0;
}

int host_event_count()
{
	return ring_buf_size_get(&host_event_ring_buf)/HOST_EVENT_ENT_SZ;
}
