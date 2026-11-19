#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT

#include <zephyr/kernel.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/dlist.h>

#include "lom_mgmt_proc_inc.h"

#include "host_event.h"


LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

struct host_event_item {
	sys_snode_t node;
	uint8_t event;
};

#define MAX_PENDING_HOST_EVENTS 16

K_FIFO_DEFINE(host_event_fifo);
K_HEAP_DEFINE(host_event_heap, sizeof(struct host_event_item) * MAX_PENDING_HOST_EVENTS);

#define HOST_EVENT_WORKQ_STACK_SIZE 2048
#define HOST_EVENT_WORKQ_PRIORITY   K_PRIO_COOP(4)

static K_KERNEL_STACK_DEFINE(host_event_workq_stack, HOST_EVENT_WORKQ_STACK_SIZE);

static struct k_work_q host_event_logger_workq;
static struct k_work host_event_logger_work;

static atomic_t host_event_logger_started;

extern int host_event_put(uint8_t event);

static void host_event_logger(struct k_work *work)
{
	struct host_event_item *item;

	while ((item = k_fifo_get(&host_event_fifo, K_NO_WAIT)) != NULL) {
		host_event_put(item->event);

		k_heap_free(&host_event_heap, item);
	}
}

int host_event_submit(uint8_t event)
{
	if (!atomic_get(&host_event_logger_started)) {
		LOG_WRN("Host event logger not started, yet! event<%d> dropped", event);
		return -1;
	}

	struct host_event_item *item = k_heap_alloc(&host_event_heap,
		sizeof(struct host_event_item), K_NO_WAIT);
	if (!item) {
		LOG_ERR("Host event heap exhausted, event<%d> dropped!", event);
		return -1;
	}

	item->event = event;

	k_fifo_put(&host_event_fifo, item);

	return k_work_submit_to_queue(&host_event_logger_workq, &host_event_logger_work);
}

int host_event_logger_init(void)
{
	k_work_queue_init(&host_event_logger_workq);

	k_work_queue_start(&host_event_logger_workq, host_event_workq_stack,
		K_KERNEL_STACK_SIZEOF(host_event_workq_stack),
		HOST_EVENT_WORKQ_PRIORITY, NULL);

	(void)k_thread_name_set(&host_event_logger_workq.thread, "LOM_MGMT_HEVTLOG");

	k_work_init(&host_event_logger_work, host_event_logger);

	atomic_set(&host_event_logger_started, 1);

	LOG_INF("Host Event Logger start OK");

	return 0;
}

SYS_INIT(host_event_logger_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#else

#include "host_event.h"

int host_event_submit(uint8_t evet)
{
	return 0;
}

#endif
