
#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/net/net_ip.h>

#include "host_event.h"

LOG_MODULE_REGISTER(host_event, LOG_LEVEL_DBG);

struct host_event_record_ent {
	uint8_t  event;
	uint32_t ticks;
} __attribute__((__packed__));


RING_BUF_DECLARE(host_event_ring_buf, CONFIG_HOST_EVENT_ENTRY_MAX * sizeof(struct host_event_record_ent));

#if 0
static uint8_t host_event_eid(uint8_t event, uint8_t status)
{
	return ((event & 0x7F) << 1) | (status & 0x1);
}
#endif

#if 0
int host_event_put(uint8_t event, uint8_t status)
{
	struct host_event_record_ent e, dummy;
	int ret;

	if (event > HOST_EVENT_MAX) {
		LOG_ERR("event out of range");
		return -1;
	}

	uint32_t ts = k_uptime_get_32();

	e.event = host_event_eid(event, status);
	e.ticks = htonl(ts);

	LOG_INF(">> add event 0x%x status %d", e.event, status);

	int ent_sz = sizeof(e);
	ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, ent_sz);
	if (ret != ent_sz) {
		LOG_ERR("Ring Buffer Full");

		/* discard the oldest entry */
		ret = ring_buf_get(&host_event_ring_buf, (uint8_t *)&dummy, ent_sz);
		if (ret != ent_sz) {
			LOG_ERR("Invalid size");
			return -1;
		}

		/* re-enqueue the entry */
		ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, ent_sz);
		if (ret != ent_sz) {
			LOG_ERR("Fatal Error");
			return -1;
		}
	}

	return ret;
}
#endif


int host_event_put(uint8_t event)
{
	struct host_event_record_ent e, dummy;
	int ret;

	if (event > HOST_EVENT_MAX) {
		LOG_ERR("event out of range");
		return -1;
	}

	uint32_t ts = k_uptime_get_32();

	e.event = event;
	e.ticks = htonl(ts);

	int ent_sz = sizeof(e);
	ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, ent_sz);
	if (ret != ent_sz) {
		LOG_ERR("Ring Buffer Full");

		/* discard the oldest entry */
		ret = ring_buf_get(&host_event_ring_buf, (uint8_t *)&dummy, ent_sz);
		if (ret != ent_sz) {
			LOG_ERR("Invalid size");
			return -1;
		}

		/* re-enqueue the entry */
		ret = ring_buf_put(&host_event_ring_buf, (const uint8_t *)&e, ent_sz);
		if (ret != ent_sz) {
			LOG_ERR("Fatal Error");
			return -1;
		}
	}

	return ret;
}


int host_event_get(uint8_t *data)
{
	int ent_sz = sizeof(struct host_event_record_ent);
	int ret;

	ret = ring_buf_get(&host_event_ring_buf, data, ent_sz);
	if (ret && ret != ent_sz) {
		LOG_ERR("Invalid size");
		return -1;
	}

	return ret;
}
