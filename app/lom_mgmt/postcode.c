#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "lom_mgmt_proc_inc.h"
#include "postcode.h"

LOG_MODULE_REGISTER(postcode_mon, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

//#define DBG_SHARED_RB

#include <zephyr/net/net_ip.h>

struct postcode_res_hdr {
	uint32_t start_time;
	uint16_t boot_count;
	uint16_t batch_size;
	uint16_t data[];
} __attribute__((__packed__));

struct postcode_ent {
	uint32_t code_time;
	uint16_t post_code;
} __attribute__((__packed__));

#define POSTCODE_ENT_SZ sizeof(struct postcode_ent)
#define POSTCODE_RES_HDR_SZ sizeof(struct postcode_res_hdr)

#ifdef DBG_SHARED_RB
/* postcode entry in ring_buf */
struct postcode_rb_ent {
	uint32_t boot_count;
	uint32_t code_time;
	uint16_t post_code;
}__attribute__((__packed__));
#define POSTCODE_RB_ENT_SZ sizeof(struct postcode_rb_ent)
#else
#define POSTCODE_RB_ENT_SZ POSTCODE_ENT_SZ
#endif

#define POSTCODE_BATCH_MAX 3

#define POSTCODE_BATCH_RB_SIZE (CONFIG_HOST_POSTCODE_ENTRY_MAX * POSTCODE_RB_ENT_SZ)

RING_BUF_DECLARE(postcode_ring_buf, POSTCODE_BATCH_RB_SIZE);

struct postcode_batch {
	uint32_t boot_count;
	uint32_t start_time;
	uint8_t  count;
};

struct postcode_batch postcode_batches[POSTCODE_BATCH_MAX];

#define POSTCODE_05_FIX
#define DIFF_TIME_PRECISION 10 /* 10ms */

static uint16_t last_postcode;

static int curr_batch_id = -1; /* this should be the latest postcode_batch */

static uint16_t postcode_batch_size(struct postcode_batch *batch)
{
	return batch->count * sizeof(struct postcode_ent);
}

static int _ring_buf_get_block(struct ring_buf* ring_buf, uint8_t* dest, uint16_t size)
{
	uint8_t *data;
	uint16_t readn;
	uint16_t count = 0;

	while (size - count) {
		readn = ring_buf_get_claim(ring_buf, &data, size - count);
		if (dest && readn) {
			memcpy(&dest[count], data, readn);
		}
		ring_buf_get_finish(&postcode_ring_buf, readn);

		count += readn;
	}

	return count;
}

#ifndef DBG_SHARED_RB
static inline int ring_buf_get_block(struct ring_buf* ring_buf, uint8_t* dest, uint16_t size)
{
	if (size == 0)
		return 0;

	if (dest == NULL) {
		LOG_ERR("No dest provided");
		return -1;
	}

	return _ring_buf_get_block(ring_buf, dest, size);
}
#endif

static inline int ring_buf_remove(struct ring_buf* ring_buf, uint16_t size)
{
	if (size == 0)
		return 0;

	return _ring_buf_get_block(ring_buf, NULL, size);
}

static int postcode_batch_data_release(int batch_id)
{
	struct postcode_batch * batch = &postcode_batches[batch_id];

	uint16_t batch_size = batch->count * POSTCODE_RB_ENT_SZ;

	LOG_DBG_PCODE("Release PC_BATCH[%d]: size %d", batch_id, batch_size);

	return ring_buf_remove(&postcode_ring_buf, batch_size);
}

static void postcode_batch_init(int batch_id, uint16_t bootcycle_cnt)
{
	postcode_batch_data_release(batch_id);

	struct postcode_batch * batch = &postcode_batches[batch_id];

	batch->boot_count = bootcycle_cnt;
	batch->start_time = k_uptime_get_32();
	batch->count = 0;
}

static void postcode_batch_reset(int batch_id)
{
	postcode_batch_data_release(batch_id);

	struct postcode_batch * batch = &postcode_batches[batch_id];

	batch->boot_count = 0;
	batch->start_time = 0;
	batch->count = 0;
}

/*
 * On rebooting, the 0x05 postcode will arrive before any other status changes.
 */
#ifdef POSTCODE_05_FIX
static inline void handle_postcode_05(uint16_t code, uint16_t* bootcycle_cnt)
{
	if (code == 0x0005 && last_postcode == 0) {
		LOG_DBG_PCODE(">> bootcycle_cnt +");
		*bootcycle_cnt += 1;
	}
}
#endif


/*
 * Release the oldest postcode batch
 */
static int postcode_batches_eviction(int self_id)
{
	/* Only has one batch(can't clear myself) */
	if (postcode_count() == 1) {
		LOG_WRN("PostCode ring buf is full, LOM dead?");
		return -1;
	}

	for (int i = 1; i < POSTCODE_BATCH_MAX; i++) {
		int id = (self_id + i) % POSTCODE_BATCH_MAX;

		if (postcode_batches[id].count) {
			LOG_DBG_PCODE("Evict PC_BATCH[%d]", id);
			postcode_batch_reset(id);
			return 0;
		}
	}

	return -1;
}

static int postcode_batch_select(uint16_t bootcycle_cnt)
{
	/*
	 * reuse current postcode_batch if has the same bootcycle_cnt
	 */
	if (curr_batch_id >= 0 && postcode_batches[curr_batch_id].boot_count == bootcycle_cnt) {
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
#ifdef DBG_SHARED_RB
	struct postcode_rb_ent ent = {bootcycle_cnt, htonl(k_uptime_get_32()), htons(code)};
#else
	struct postcode_ent ent = {htonl(k_uptime_get_32()), htons(code)};
#endif

	ret = ring_buf_put(&postcode_ring_buf, (const uint8_t *)&ent, sizeof(ent));
	if (ret != sizeof(ent)) {
		if (postcode_batches_eviction(curr_batch_id) < 0) {
			return -ENOMEM;
		}

		/* re-put */
		ret = ring_buf_put(&postcode_ring_buf, (const uint8_t *)&ent, sizeof(ent));
		if (ret != sizeof(ent)) {
			LOG_WRN("Evicted, no memory for new postcode!!");
			return -ENOMEM;
		}
	}

	LOG_DBG_PCODE("PC_BATCH[%u]+: [%d][%03d][%08x], 0x%04x", curr_batch_id,
		pb->boot_count, pb->count, ntohl(ent.code_time), ntohs(ent.post_code));

	last_postcode = code;

	pb->count++;

	avail_resource_set(AVAIL_RES_POSTCODE, 1);

	return 0;
}

static inline int batch_size_adjust(uint16_t batch_size, uint16_t curr_res_used,
	uint16_t max_size, uint16_t* new_size)
{
	/* if no space for one entry */
	if (curr_res_used + POSTCODE_RES_HDR_SZ + POSTCODE_ENT_SZ > max_size) {
		return -1;
	}

	/* if no enough space for all the entries */
	if (curr_res_used + POSTCODE_RES_HDR_SZ + batch_size > max_size) {
		*new_size = ((max_size - curr_res_used - POSTCODE_RES_HDR_SZ) /
			POSTCODE_ENT_SZ * POSTCODE_ENT_SZ);

		LOG_DBG_PCODE("batch size adjust: %d -> %d", batch_size, *new_size);
	}

	return 0;
}

#ifdef DBG_SHARED_RB
int postcode_get(uint8_t *buf, uint16_t buf_size, uint16_t *ret_size)
{
	/* no postcode data */
	if (curr_batch_id < 0) {
		*ret_size = 0;
		return 0;
	}

	/* The next batch should be the oldest batch */
	int batch_id = (curr_batch_id + 1) % POSTCODE_BATCH_MAX;

	uint16_t off = 0;

	//LOG_HEXDUMP_ERR(postcode_ring_buf.buffer, 60, "RING");

	for (int i = 0; i < POSTCODE_BATCH_MAX; i++) {
		struct postcode_batch *pb = &postcode_batches[batch_id];
		uint16_t batch_size = postcode_batch_size(pb);

		LOG_DBG_PCODE("Process PC_BATCH[%u]: boot[%u] size[%03u] time[%08x] count %d",
			batch_id, pb->boot_count, batch_size, pb->start_time, pb->count);

		if (batch_size) {
			struct postcode_res_hdr* prh = (struct postcode_res_hdr*)&buf[off];
			uint16_t real_batch_size = batch_size;

			if (batch_size_adjust(batch_size, off, buf_size, &real_batch_size) < 0) {
				break;
			}

			prh->start_time = htonl(pb->start_time);
			prh->boot_count = htons(pb->boot_count);
			prh->batch_size = htons(real_batch_size);

			struct postcode_rb_ent rbe;
			uint16_t readn;
			uint16_t count = real_batch_size/sizeof(struct postcode_ent);
			struct postcode_ent *pent = (struct postcode_ent *)prh->data;

			for (int n = 0; n < count; n++, pent++) {
				readn = ring_buf_get(&postcode_ring_buf, (uint8_t *)&rbe, sizeof(rbe));
				if (readn != sizeof(rbe)) {
					LOG_ERR("Get postcode failed");
					return -EIO;
				}

				//LOG_HEXDUMP_ERR(&e, sizeof(e), "DDD");

				if (rbe.boot_count != pb->boot_count) {
					LOG_HEXDUMP_ERR(&rbe, sizeof(rbe), "RBE");
					return -EIO;
				}

				pent->code_time = rbe.code_time;
				pent->post_code = rbe.post_code;

				//LOG_HEXDUMP_ERR(pent, sizeof(*pent), "RET");
			}

			pb->count -= count;

			off += sizeof(*prh) + real_batch_size;

			if (real_batch_size != batch_size) {
				LOG_WRN("PostCode: res buff full quit");
				break;
			}
		}

		/* go to the next batch */
		batch_id = (batch_id + 1) % POSTCODE_BATCH_MAX;
	}

	*ret_size = off;

	if (!postcode_count()) {
		avail_resource_set(AVAIL_RES_POSTCODE, 0);
	}

	return 0;
}
#else
int postcode_get(uint8_t *buf, uint16_t buf_size, uint16_t *ret_size)
{
	/* no postcode data */
	if (curr_batch_id < 0) {
		*ret_size = 0;
		return 0;
	}

	/* The next batch should be the oldest batch */
	int batch_id = (curr_batch_id + 1) % POSTCODE_BATCH_MAX;

	uint16_t off = 0;

	//LOG_HEXDUMP_ERR(postcode_ring_buf.buffer, 60, "RING");

	for (int i = 0; i < POSTCODE_BATCH_MAX; i++) {
		struct postcode_batch *pb = &postcode_batches[batch_id];
		uint16_t batch_size = postcode_batch_size(pb);

		LOG_DBG_PCODE("Process PC_BATCH[%u]: boot[%u] size[%03u] time[%08x] count %d",
			batch_id, pb->boot_count, batch_size, pb->start_time, pb->count);

		if (batch_size) {
			struct postcode_res_hdr* prh = (struct postcode_res_hdr*)&buf[off];
			uint16_t real_batch_size = batch_size;

			if (batch_size_adjust(batch_size, off, buf_size, &real_batch_size) < 0) {
				break;
			}

			prh->start_time = htonl(pb->start_time);
			prh->boot_count = htons(pb->boot_count);
			prh->batch_size = htons(real_batch_size);

			ring_buf_get_block(&postcode_ring_buf, (uint8_t*)prh->data, real_batch_size);

			pb->count -= real_batch_size/sizeof(struct postcode_ent);

			off += sizeof(*prh) + real_batch_size;

			if (real_batch_size != batch_size) {
				LOG_WRN("PostCode: res buff full quit");
				break;
			}
		}

		/* go to the next batch */
		batch_id = (batch_id + 1) % POSTCODE_BATCH_MAX;
	}

	*ret_size = off;

	if (!postcode_count()) {
		avail_resource_set(AVAIL_RES_POSTCODE, 0);
	}

	return 0;
}
#endif

int postcode_count()
{
	uint16_t count = 0;

	for (int i = 0; i < POSTCODE_BATCH_MAX; i++) {
		struct postcode_batch *pb = &postcode_batches[i];
		count += pb->count;
	}

	return count;
}

#endif
