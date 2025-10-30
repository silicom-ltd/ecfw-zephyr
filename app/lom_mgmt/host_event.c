
#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/net/net_ip.h>

#include "host_event.h"

LOG_MODULE_REGISTER(host_event, LOG_LEVEL_DBG);

struct host_event_record_ent {
	uint8_t  event;
	uint32_t ticks;
} __attribute__((__packed__));

#define HOST_EVENT_ENT_SZ sizeof(struct host_event_record_ent)

RING_BUF_DECLARE(host_event_ring_buf,
	CONFIG_HOST_EVENT_ENTRY_MAX * HOST_EVENT_ENT_SZ);

int host_event_put(uint8_t event)
{
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
	if (ret != HOST_EVENT_ENT_SZ) {
		LOG_DBG("Ring Buffer Full");

		/* discard the oldest entry */
		ret = ring_buf_get(&host_event_ring_buf, (uint8_t *)&dummy, HOST_EVENT_ENT_SZ);
		if (ret != HOST_EVENT_ENT_SZ) {
			LOG_ERR("Discard failed");
			return -1;
		}

		/* re-enqueue the entry */
		ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, HOST_EVENT_ENT_SZ);
		if (ret != HOST_EVENT_ENT_SZ) {
			LOG_ERR("Put Error");
			return -1;
		}
	}

	return 0;
}


int host_event_get(uint8_t *data)
{
	int ret;

	ret = ring_buf_get(&host_event_ring_buf, data, HOST_EVENT_ENT_SZ);
	if (ret && ret != HOST_EVENT_ENT_SZ) {
		LOG_ERR("Invalid size");
		return -1;
	}

	return ret;
}
