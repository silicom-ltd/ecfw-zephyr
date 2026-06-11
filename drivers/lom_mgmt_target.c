/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT i2c_target_lom_mgmt

#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/drivers/i2c.h>
#include <string.h>
#include <assert.h>

#include <zephyr/logging/log.h>

#include "lom_mgmt_i2c.h"

LOG_MODULE_REGISTER(lom_mgmt_i2c, CONFIG_LOM_MGMT_I2C_LOG_LEVEL);

//#define DEBUG_TEST

#ifdef CONFIG_LOM_MGMT_I2C_DBG_I2C
#define LOG_DBG_I2C(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_I2C(...) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_I2C_DBG_STA
#define LOG_DBG_STA(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_STA(...) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_I2C_DBG_APP
#define LOG_DBG_APP(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_APP(...) (void)0
#endif

#define I2C_BLK_SZ 32

#define I2C_REQ_ADDR 0xFE
#define I2C_FNI_ADDR 0xEF

#define COMM_RES_FREE_ZONE 0
#define COMM_RES_PROC_STAT 1
#define COMM_RES_DATA_BASE 2

#define POWER_CTRL_REBOOT 1

#define COM_META_REQ 0x1
#define COM_META_RES 0x2
#define COM_META_EXT 0x4
#define COM_META_ALL 0x7

#define ERR_RD_RET_DFL 0
#define ERR_WR_RET_DFL 0

#define ACCESS_WR 1
#define ACCESS_RD 0

/*
 * STA_RECV_REQ -> STA_WAIT_PROC, STA_RECV_FINI:
 *   completed in one i2c block write/read, so the Linux kernel on the LOM
 *   can ensures that these processes are not interrupted.
 *
 * STA_SEND_RES might contains multiple i2c block reads, this might be interrupted.
 */
enum ctx_sta_stages {
	STA_INIT = 0,
	STA_NO_PROC,
	STA_READY,
	STA_RECV_REQ,
	STA_WAIT_PROC,
	STA_SEND_RES,
	STA_RECV_FINI,
};

#if defined(CONFIG_LOM_MGMT_I2C_DBG_STA) || defined(CONFIG_LOM_MGMT_I2C_DBG_APP) || \
	defined(CONFIG_LOM_MGMT_I2C_DBG_I2C_DAT)
/*
 * ctx_sta_stages's short name, used for debug
 */
static char * ctx_sta_string[] = {
	"INIT",
	"NOPC",
	"RADY",
	"RREQ",
	"PROC",
	"SRES",
	"RFNI",
};
#endif

struct lom_mgmt_res_hdr {
	uint8_t  code;
	uint16_t meta;
} __attribute__((__packed__));

struct lom_mgmt_i2c_context {
	bool first_write;

	uint8_t state;
	uint8_t state_last; /* used in state_restore() */

	uint8_t first_data_access; /* control executing of prepare_data_access() */

	uint8_t access_addr;

	/*
	 * Request's meta data
	 */
	uint16_t req_idx;       /* index for lom_mgmt_req.buf */
	uint8_t  req_func_last; /* used for func FINI */

	/*
	 * Response's meta data
	 */
	uint16_t res_idx;       /* index for lom_mgmt_res.buf */

	/*
	 * App's data
	 */
	struct lom_mgmt_i2c_callbacks *cb;

	uint8_t rdy_sigs;

	uint8_t res_ready; /* set by EC-LOM PROC */

	struct lom_mgmt_req req;
	struct lom_mgmt_res res;
};


struct i2c_lom_mgmt_target_data {
	struct i2c_target_config config;
	struct lom_mgmt_i2c_context ctx;
};

struct i2c_lom_mgmt_target_config {
	struct i2c_dt_spec bus;
};

#define LOM_MGMT_CTX_FROM_CFG(config)					\
	&(CONTAINER_OF(config, struct i2c_lom_mgmt_target_data, config)->ctx)

#define LOM_MGMT_CTX_FROM_DEV(dev)				\
	&(((struct i2c_lom_mgmt_target_data *)dev->data)->ctx)

static int lom_mgmt_func_cap[FUNC_COUNT] = {
	[FUNC_FINI] = 1,

#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
	[FUNC_POWER_CTRL] = 1,
#endif

	[FUNC_GET_ACPI_POWER_STATE] = 1,
	[FUNC_GET_SENSORS] = 1,

#ifdef CONFIG_LOM_MGMT_FUNC_FRU
	[FUNC_GET_FRU] = 1,
#endif
#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT
	[FUNC_GET_EVENTS] = 1,
#endif
#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
	[FUNC_GET_POSTCODE] = 1,
#endif

	[FUNC_TEST_L3] = 1,
};

static void lom_mgmt_tgt_deliver_request(struct lom_mgmt_i2c_context *ctx);

static void set_ctx_response_meta(struct lom_mgmt_i2c_context* ctx,
	uint8_t code, uint16_t dat_size, uint8_t flag, uint8_t state);

static int lom_mgmt_func_is_supported(int func)
{
	return lom_mgmt_func_cap[func];
}

static inline int set_response_info(struct lom_mgmt_res* res,
	uint8_t code, uint16_t dlen_or_sys_error, uint8_t flag)
{
	uint16_t dlen = 0;

	if (code == EC_RET_OK) {
		if (dlen_or_sys_error > RES_DLEN_MAX) {
			LOG_ERR("data size exceeds the limit %d", RES_DLEN_MAX);
			return -1;
		}
		dlen = dlen_or_sys_error;
	}

	res->code = code;
	res->meta = htons((dlen_or_sys_error & 0xFFF) | ((uint16_t)(flag & 0xF) << 12));
	res->size = dlen + RES_HEAD_LEN;

	return 0;
}

int lom_mgmt_i2c_set_callbacks(const struct device *dev, struct lom_mgmt_i2c_callbacks *cb)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_DEV(dev);

	ctx->cb = cb;
	if (ctx->cb == NULL) {
		LOG_ERR("LOM_MGMT_PROC callback is NULL");
		return -1;
	}

	LOG_INF("LOM_MGMT_PROC Registered");

	set_ctx_response_meta(ctx, EC_RET_READY, 0, 0, STA_READY);

	return 0;
}

int lom_mgmt_i2c_set_avail_res(const struct device *dev, uint8_t bit, uint8_t val)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_DEV(dev);

	ctx->rdy_sigs = (ctx->rdy_sigs & ~(1 << bit)) | ((val & 0x1) << bit);

	return 0;
}

int lom_mgmt_i2c_response_ready(const struct device *dev,
	uint8_t code, uint16_t dlen_or_sys_error, uint8_t flag)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_DEV(dev);

	if (ctx->state != STA_WAIT_PROC) {
		LOG_ERR("F(%d) Res Ready: Error State %d!!", ctx->req_func_last, ctx->state);
		return -1;
	}

	set_ctx_response_meta(ctx, code, dlen_or_sys_error, flag, STA_SEND_RES);

	ctx->res_ready = 1;

	LOG_DBG_APP("F(%d) Res Ready: code:%d, meta(n):0x%04x,(h)(F:0x%x,L/E:%d), size:%d",
		ctx->req_func_last, ctx->res.code,
		ctx->res.meta, RES_META_FLAG_NTOH(ctx->res.meta), RES_META_DLEN_NTOH(ctx->res.meta),
		ctx->res.size);

	return 0;
}

static inline void ctx_meta_data_reset(struct lom_mgmt_i2c_context *ctx, int meta_sel)
{
	if (meta_sel & COM_META_EXT) {
		ctx->req_func_last = 0;
		ctx->access_addr = 0;
	}

	if (meta_sel & COM_META_REQ) {
		ctx->req_idx = 0;
		ctx->req.size = 0;
	}

	if (meta_sel & COM_META_RES) {
		ctx->res_idx = 0;
		ctx->res.size = 0;
	}
}

/*
 * Unexpected write operations to FINI_ADDR may corrupt
 * operations requiring multiple read operations, necessitating state recovery.
 */
static inline void state_update(struct lom_mgmt_i2c_context * ctx, uint8_t new_state)
{
	ctx->state_last = ctx->state;
	ctx->state = new_state;
}

static inline void state_restore(struct lom_mgmt_i2c_context * ctx)
{
	LOG_DBG_STA("(S) Restore: [%s] -> [%s]",
		ctx_sta_string[ctx->state], ctx_sta_string[ctx->state_last]);

	/* this should'nt happen */
	if (ctx->state_last != STA_SEND_RES && ctx->state_last != STA_WAIT_PROC &&
		ctx->state_last != STA_READY) {
		LOG_WRN("STA error %d", ctx->state_last);
	}

	ctx->state = ctx->state_last;
}

static int lom_mgmt_tgt_wr_req(struct i2c_target_config *config)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_CFG(config);

	ctx->first_write = true;

	LOG_DBG_I2C("first_write=%d", ctx->first_write);

	return 0;
}

static inline int need_timestamp(uint16_t res_meta)
{
	return RES_META_FLAG_NTOH(res_meta) & RES_META_F_TIMESTAMP;
}

static inline void set_timestamp(uint8_t *buf)
{
	uint32_t ts = k_uptime_get_32();

	buf[0] = (ts >> 24) & 0xFF;
	buf[1] = (ts >> 16) & 0xFF;
	buf[2] = (ts >>  8) & 0xFF;
	buf[3] = (ts >>  0) & 0xFF;
}

static int prepare_data_access(struct lom_mgmt_i2c_context *ctx, int mode)
{
	if (ctx->first_data_access == 0)
		return 0;

	ctx->first_data_access = 0;

	/* LOM want to write */
	if (mode == ACCESS_WR) {
		if (ctx->access_addr == I2C_REQ_ADDR) {
			if (ctx->state == STA_READY) {
				state_update(ctx, STA_RECV_REQ);

				LOG_DBG_STA("(S) [Wr] [%s] -> [%s]",
					ctx_sta_string[ctx->state_last], ctx_sta_string[ctx->state]);
				return 0;
			}
		}
		else if (ctx->access_addr == I2C_FNI_ADDR) {
			if (ctx->state > STA_NO_PROC) {
				/* prepare for recv FINI command */
				ctx_meta_data_reset(ctx, COM_META_REQ);
				state_update(ctx, STA_RECV_FINI);

				LOG_DBG_STA("(S) [Wr] [%s] -> [%s]",
					ctx_sta_string[ctx->state_last], ctx_sta_string[ctx->state]);

				return 0;
			}
		}
	}
	/* LOM want to read */
	else {
		if (ctx->access_addr == COMM_RES_FREE_ZONE) {
			/* only returns rdy_sigs, now */
			return 0;
		}
		else if (ctx->access_addr == COMM_RES_PROC_STAT) {
			ctx->res_idx = 0;
		}
		else {
#ifndef DEBUG_TEST
			/* Minimize the impact of unintended read operations on the system */
			if (ctx->state != STA_SEND_RES) {
				goto error_quit;
			}
#endif

			uint16_t idx = RES_HEAD_LEN + (ctx->access_addr - 2) * I2C_BLK_SZ;
#ifndef DEBUG_TEST
			/* Unexpected access from LOM, such as i2cdump, will trigger this checking. */
			if (idx >= ctx->res.size) {
				goto error_quit;
			}
#endif

			ctx->res_idx = idx;

			if (ctx->res_ready) {
				if (need_timestamp(ctx->res.meta)) {
					set_timestamp(ctx->res.data);
				}

				ctx->res_ready = 0;
			}
		}

		LOG_DBG_STA("(S) [Rd] RES[%d/%d]", ctx->res_idx, ctx->res.size-1);

		return 0;
	}

error_quit:
	LOG_DBG_STA("(S) Error: STA[%s], %s @ COMM[0x%02x]",
		ctx_sta_string[ctx->state], mode == ACCESS_WR ? "Wr" : "Rd",
		ctx->access_addr);

	ctx->access_addr = 0;

	return -1;
}

static int lom_mgmt_tgt_wr_rcv(struct i2c_target_config *config, uint8_t val)
{
	struct lom_mgmt_i2c_context * ctx = LOM_MGMT_CTX_FROM_CFG(config);

	if (ctx->first_write) {
		ctx->first_write = false;

		/*
		 * record the comm value for prepare_data_access()
		 */
		ctx->access_addr = val;

		ctx->first_data_access = 1;

#if (defined(_DBG_STA) || defined(_DBG_I2C_DAT)) && (CONFIG_LOM_MGMT_I2C_LOG_LEVEL >= LOG_LEVEL_DBG)
		LOG_INF("(+) New Access @ COMM[0x%02x]", ctx->access_addr);
#endif
	}
	else { /* this might be the request message */
		if (prepare_data_access(ctx, ACCESS_WR) < 0) {
			return 0;
		}

		if (ctx->state != STA_RECV_REQ && ctx->state != STA_RECV_FINI) {
			return 0;
		}

		if (ctx->req_idx < REQ_BUFF_SIZE) {
			LOG_DBG_I2C("COMM[0x%02x] REQ[%d] <= 0x%02x", ctx->access_addr, ctx->req_idx, val);
			ctx->req.buf[ctx->req_idx++] = val;
		}
	}

	return 0;
}

/*
 * This function's return will be ignored by upper framework
 */
static int lom_mgmt_tgt_stop(struct i2c_target_config *config)
{
	struct lom_mgmt_i2c_context * ctx = LOM_MGMT_CTX_FROM_CFG(config);

#if (defined(_DBG_STA) || defined(_DBG_I2C_DAT)) && (CONFIG_LOM_MGMT_I2C_LOG_LEVEL >= LOG_LEVEL_DBG)
	LOG_INF("Stop: COMM[0x%02x], REQ[idx %d, size %d], STA[%s]",
		ctx->access_addr, ctx->req_idx, ctx->req.size, ctx_sta_string[ctx->state]);
#endif

	if (ctx->state == STA_RECV_REQ || ctx->state == STA_RECV_FINI) {
		ctx->req.size = ctx->req_idx;
		lom_mgmt_tgt_deliver_request(ctx);
	}

	ctx_meta_data_reset(ctx, COM_META_REQ);

	ctx->access_addr = 0;

	return 0;
}

/*
 * i2c_smbus_read_byte() will reach here directly
 */
static int lom_mgmt_tgt_rd_req(struct i2c_target_config *config, uint8_t *val)
{
	struct lom_mgmt_i2c_context * ctx = LOM_MGMT_CTX_FROM_CFG(config);

	if (prepare_data_access(ctx, ACCESS_RD) < 0) {
		return 0;
	}

	if (ctx->access_addr == 0) {
		*val = ctx->rdy_sigs;
	}
	else {
		__ASSERT_NO_MSG(ctx->res.size && ctx->res_idx < ctx->res.size);

		*val = ctx->res.buf[ctx->res_idx];

#ifdef DEBUG_TEST
		if (ctx->res_idx >= 7 && (*val != (ctx->res_idx - 3) % 256))
			LOG_DBG(">> OOPS 0");
#endif
	}

	LOG_DBG_I2C("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);
	//LOG_WRN("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);

	return 0;
}

/*
 * Only transaction via lom_mgmt_tgt_wr_rcv() will reach here
 */
static int lom_mgmt_tgt_rd_rcv(struct i2c_target_config *config, uint8_t *val)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_CFG(config);

	if (ctx->access_addr == 0) {
		return 0;
	}

	__ASSERT_NO_MSG(ctx->res.size && ctx->res_idx < ctx->res.size);

	ctx->res_idx = (ctx->res_idx + 1) % ctx->res.size;

	*val = ctx->res.buf[ctx->res_idx];

#ifdef DEBUG_TEST
	if (ctx->res_idx >= 7 && (*val != (ctx->res_idx - 3) % 256))
		LOG_DBG(">> OOPS 1");
#endif

	LOG_DBG_I2C("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);
	//LOG_WRN("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);

	return 0;
}

static void set_ctx_response_meta(struct lom_mgmt_i2c_context* ctx,
	uint8_t code, uint16_t dlen_or_sys_error, uint8_t flag, uint8_t state)
{
	if (set_response_info(&ctx->res, code, dlen_or_sys_error, flag) < 0) {
		return;
	}

	LOG_DBG_STA("(S) [%s] -> [%s]", ctx_sta_string[ctx->state], ctx_sta_string[state]);

	ctx->state = state;
}

static bool check_csum(struct lom_mgmt_i2c_context *ctx)
{
	uint8_t csum;
	uint8_t csum_org;
	int i;

	csum_org = ctx->req.csum;
	ctx->req.csum = 0;

	for (csum = 0, i = 0; i < ctx->req.size; i++) {
		csum += ctx->req.buf[i];
	}

	if (csum != csum_org) {
		//LOG_DBG_APP("invalid csum 0x%02x, should be 0x%02x", csum_org, csum);
		return false;
	}

	return true;
}

static void lom_mgmt_tgt_do_fini(struct lom_mgmt_i2c_context *ctx)
{
	LOG_DBG_APP("FINI: Last Func<%02d> STA[%s] RES[(FYI)%4d/%-4d]", ctx->req_func_last,
		ctx_sta_string[ctx->state_last], ctx->res_idx, ctx->res.size-1);

	ctx->cb->send_cancel();

	ctx_meta_data_reset(ctx, COM_META_ALL);

	set_ctx_response_meta(ctx, EC_RET_READY, 0, 0, STA_READY);

	ctx->req_func_last = 0;
}


/*
 *  ALERT: this will delay the I2C bus, make the LOM side timeout.
 */
static void lom_mgmt_tgt_do_testl2(struct lom_mgmt_i2c_context *ctx)
{
	uint16_t tst_size = ((uint16_t)ctx->req.data[0] << 8) | ctx->req.data[1]; /* test size */

	LOG_DBG_APP("test_l2 size %d\n", tst_size);

	if (tst_size > (RES_DLEN_MAX)) {
		LOG_ERR("request size exceeds the limit: %d", RES_DLEN_MAX);
		set_ctx_response_meta(ctx, EC_RET_ERR_INV_SIZE, 0, 0, STA_SEND_RES);
	}
	else {
		for (int i = 0; i < tst_size; i++) {
			ctx->res.data[i] = i + RES_HEAD_LEN;
		}
		set_ctx_response_meta(ctx, EC_RET_OK, tst_size, 0, STA_SEND_RES);
	}

	ctx->res_ready = 1;
}

static void lom_mgmt_tgt_deliver_request(struct lom_mgmt_i2c_context *ctx)
{
	LOG_DBG_APP("Request: size %d, func %d, dlen %d, csum 0x%02x",
		ctx->req.size, ctx->req.func, ctx->req.dlen, ctx->req.csum);

	/*
	 * Only valid requests will have an impact on the system
	 */
	if (ctx->req.size < REQ_HEAD_LEN || ctx->req.dlen > REQ_DLEN_MAX ||
		(ctx->req.dlen != ctx->req.size - REQ_HEAD_LEN)) {
		LOG_DBG_APP("Invalid request size: %u", ctx->req.size);
		goto error_quit;
	}

	if (check_csum(ctx) == false) {
		LOG_DBG_APP("Invalid request csum");
		goto error_quit;
	}

	if (ctx->req.func < FUNC_FIRST || ctx->req.func > FUNC_LAST)
	{
		LOG_DBG_APP("Invalid func %u", ctx->req.func);
		goto error_quit;
	}

	if ((ctx->state == STA_RECV_FINI && ctx->req.func != FUNC_FINI) ||
		(ctx->state == STA_RECV_REQ && ctx->req.func == FUNC_FINI))
	{
		LOG_DBG_APP("Un-expected func %d", ctx->req.func);
		goto error_quit;
	}

	if (ctx->req.func == FUNC_FINI) {
		lom_mgmt_tgt_do_fini(ctx);
		return;
	}

	ctx->req_func_last = ctx->req.func;
	ctx->res_ready = 0;

	if (lom_mgmt_func_is_supported(ctx->req.func) == 0) {
		set_ctx_response_meta(ctx, EC_RET_ERR_NOT_IMPL, 0, 0, STA_SEND_RES);
		return;
	}

	if (ctx->req.func == FUNC_TEST_L2) {
		lom_mgmt_tgt_do_testl2(ctx);
	}
	else {
		if (ctx->cb == NULL) { /* this shouldn't happen */
			set_ctx_response_meta(ctx, EC_RET_NO_PROC, 0, 0, STA_SEND_RES);
			LOG_WRN("State corrupted");
			return;
		}

		set_ctx_response_meta(ctx, EC_RET_RETRY, 0, 0, STA_WAIT_PROC);

		ctx->cb->send_request(&ctx->req, &ctx->res);

		LOG_DBG_APP("Send F<%d> to app", ctx->req.func);
	}

	return;

error_quit:
	//LOG_HEXDUMP_DBG(ctx->req.buf, REQ_HEAD_LEN, "ReqHdr");
	state_restore(ctx);
	return;
}

static int lom_mgmt_target_register(const struct device *dev)
{
	const struct i2c_lom_mgmt_target_config *cfg = dev->config;
	struct i2c_lom_mgmt_target_data *data = dev->data;

	return i2c_target_register(cfg->bus.bus, &data->config);
}

static int lom_mgmt_target_unregister(const struct device *dev)
{
	const struct i2c_lom_mgmt_target_config *cfg = dev->config;
	struct i2c_lom_mgmt_target_data *data = dev->data;

	return i2c_target_unregister(cfg->bus.bus, &data->config);
}

static const struct i2c_target_driver_api api_funcs = {
	.driver_register   = lom_mgmt_target_register,
	.driver_unregister = lom_mgmt_target_unregister,
};

static const struct i2c_target_callbacks lom_mgmt_callbacks = {
	.write_requested = lom_mgmt_tgt_wr_req,
	.write_received  = lom_mgmt_tgt_wr_rcv,
	.read_requested  = lom_mgmt_tgt_rd_req,
	.read_processed  = lom_mgmt_tgt_rd_rcv,
	.stop            = lom_mgmt_tgt_stop,
};

static int i2c_lom_mgmt_target_init(const struct device *dev)
{
	struct i2c_lom_mgmt_target_data *data = dev->data;
	const struct i2c_lom_mgmt_target_config *cfg = dev->config;

	if (!device_is_ready(cfg->bus.bus)) {
		LOG_ERR("I2C controller device not ready");
		return -ENODEV;
	}

	data->config.address = cfg->bus.addr;
	data->config.callbacks = &lom_mgmt_callbacks;

	//data->ctx.rdy_sigs = 0;
	//data->ctx.access_addr = 0;

	set_ctx_response_meta(&data->ctx, EC_RET_NO_PROC, 0, 0, STA_NO_PROC);

	if (lom_mgmt_target_register(dev) < 0)
		LOG_ERR("%s, Register failed", __func__);
	else
		LOG_INF("LOM_MGMT I2C target at addr 0x%02x", cfg->bus.addr);

	return 0;
}

#define I2C_LOM_MGMT_INIT(inst)						\
	static struct i2c_lom_mgmt_target_data				\
		i2c_lom_mgmt_target_##inst##_dev_data;			\
									\
	static const struct i2c_lom_mgmt_target_config			\
		i2c_lom_mgmt_target_##inst##_cfg = {			\
		.bus = I2C_DT_SPEC_INST_GET(inst),			\
	};								\
									\
	DEVICE_DT_INST_DEFINE(inst,					\
			    &i2c_lom_mgmt_target_init,			\
			    NULL,			\
			    &i2c_lom_mgmt_target_##inst##_dev_data,	\
			    &i2c_lom_mgmt_target_##inst##_cfg,		\
			    POST_KERNEL,				\
			    CONFIG_I2C_TARGET_INIT_PRIORITY,		\
			    &api_funcs);

DT_INST_FOREACH_STATUS_OKAY(I2C_LOM_MGMT_INIT)
