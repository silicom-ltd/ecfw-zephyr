#ifdef CONFIG_POSTCODE_MONITOR

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "postcode.h"

LOG_MODULE_REGISTER(postcode_mon, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define POSTCODE_BATCH_MAX 3
#define POSTCODE_ENT_MAX_PER_BATCH CONFIG_HOST_POSTCODE_ENT_MAX_PER_BATCH

#define DIFF_TIME_PRECISION 10 /* 10ms */

#define POSTCODE_BATCH_BUF_SIZE_MAX (POSTCODE_ENT_MAX_PER_BATCH * POSTCODE_ENT_SZ)

RING_BUF_DECLARE(postcode_ring_buf0, POSTCODE_BATCH_BUF_SIZE_MAX);
RING_BUF_DECLARE(postcode_ring_buf1, POSTCODE_BATCH_BUF_SIZE_MAX);
RING_BUF_DECLARE(postcode_ring_buf2, POSTCODE_BATCH_BUF_SIZE_MAX);

struct postcode_batch {
	uint32_t boot_count;
	uint32_t start_time;
	uint8_t  count;
	struct ring_buf * codes;
};

struct postcode_batch postcode_batches[3] = {
	{.codes = &postcode_ring_buf0},
	{.codes = &postcode_ring_buf1},
	{.codes = &postcode_ring_buf2},
};

static uint16_t last_postcode;

static int curr_batch_id = -1; /* this should be the latest postcode_batch */

static void postcode_batch_init(int batch_id, uint16_t bootcycle_cnt)
{
	struct postcode_batch * batch = &postcode_batches[batch_id];

	batch->boot_count = bootcycle_cnt;
	batch->start_time = k_uptime_get_32();
	batch->count = 0;

	ring_buf_reset(batch->codes);
}

/*
 * On rebooting, the 0x05 postcode will arrive before any other status changes.
 */
#ifdef POSTCODE_05_FIX
static inline void handle_postcode_05(uint16_t code, uint16_t* bootcycle_cnt)
{
	if (code == 0x0005 && last_postcode == 0) {
		*bootcycle_cnt += 1;
	}
}
#endif

static int postcode_batch_select(uint16_t bootcycle_cnt)
{
	/*
	 * reuse current postcode_batch if has the same bootcycle_cnt
	 */
	if (curr_batch_id >= 0 && postcode_batches[curr_batch_id].boot_count == bootcycle_cnt) {
		if (ring_buf_space_get(postcode_batches[curr_batch_id].codes) == 0) {
			LOG_ERR("the %dth postcode_batch is full", curr_batch_id);
			return -1;
		}

		return 0;
	}

	/*
	 * new bootcycle_cnt, go to next postcode_batch and overide it
	 */
	int new_batch_id = (curr_batch_id + 1) % POSTCODE_BATCH_MAX;

	postcode_batch_init(new_batch_id, bootcycle_cnt);

	curr_batch_id = new_batch_id;

	return 0;
}

int postcode_add(uint16_t code, uint16_t bootcycle_cnt)
{
	int ret;
#ifdef POSTCODE_05_FIX
	handle_postcode_05(code, &bootcycle_cnt);
#endif

	if (postcode_batch_select(bootcycle_cnt) < 0)
		return -1;

	struct postcode_batch *pb = &postcode_batches[curr_batch_id];

	/* store with network order */
	struct postcode_ent ent = {htonl(k_uptime_get_32()), htons(code)};

	ret = ring_buf_put(pb->codes, (const uint8_t *)&ent, POSTCODE_ENT_SZ);
	if (ret != POSTCODE_ENT_SZ) {
		LOG_ERR("No memory to store postcode");
		return -ENOMEM;
	}

	LOG_DBG(">> PC_BATCH[%u]+: [%d][%03d][%08x], 0x%04x", curr_batch_id,
		pb->boot_count, pb->count, ntohl(ent.code_time), ntohs(ent.post_code));

	last_postcode = code;

	pb->count++;

	return 0;
}

int postcode_get(uint8_t *ret, uint16_t *ret_size)
{
	uint16_t ret_off = 0;

	/* no postcode data */
	if (curr_batch_id < 0) {
		*ret_size = 0;
		return 0;
	}

	/* The next batch should be the oldest batch */
	int batch_id = (curr_batch_id + 1) % POSTCODE_BATCH_MAX;

	for (int i = 0; i < POSTCODE_BATCH_MAX; i++) {
		struct postcode_batch *pb = &postcode_batches[batch_id];
		uint16_t batch_size = ring_buf_size_get(pb->codes);

		LOG_DBG(">> Process PC_BATCH[%u]: boot[%u] size[%03u] time[%08x] count %d",
			batch_id, pb->boot_count, batch_size, pb->start_time, pb->count);

		if (batch_size) {
			struct postcode_res_hdr* prh = (struct postcode_res_hdr*)&ret[ret_off];

			prh->start_time = htonl(pb->start_time);
			prh->boot_count = htons(pb->boot_count);
			prh->batch_size = htons(batch_size);

			uint32_t readn;
			uint8_t *data;
			uint16_t copyn = 0;
			while (batch_size - copyn) {
				readn = ring_buf_get_claim(pb->codes, &data, batch_size - copyn);
				memcpy(&prh->data[copyn], data, readn);
				ring_buf_get_finish(pb->codes, readn);

				copyn += readn;
			}

			pb->count -= batch_size/sizeof(struct postcode_ent);

			ret_off += sizeof(*prh) + batch_size;
		}

		/* go to the next batch */
		batch_id = (batch_id + 1) % POSTCODE_BATCH_MAX;
	}

	*ret_size = ret_off;

	return 0;
}

#endif
