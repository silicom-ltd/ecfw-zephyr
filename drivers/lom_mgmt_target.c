/*
 * Copyright (c) 2017 BayLibre, SAS
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

//#define DEBUG_TEST
//#define _DBG_I2C_DAT_
//#define _DBG_STA_

#ifndef _DBG_I2C_DAT_
#define LOG_DBG_I2C_DAT(...) (void)0
#else
#define LOG_DBG_I2C_DAT(...) LOG_DBG(__VA_ARGS__)
#endif

#ifndef _DBG_STA_
#define LOG_DBG_STA(...) (void)0
#else
#define LOG_DBG_STA(...) LOG_DBG(__VA_ARGS__)
#endif

LOG_MODULE_REGISTER(lom_mgmt_i2c, CONFIG_LOM_MGMT_I2C_LOG_LEVEL);

#define I2C_REQ_ADDR 0xFE
#define I2C_FNI_ADDR 0xEF

#define COMM_RES_STAT 1
#define COMM_RES_DATA 2

static char * err_string[] = {
	"INV",
	"No Process",
	"Ready",
	"Retry",
	"OK",
	"Invalid request payload size",
	"Invalid request func",
	"Invalid request csum",
};

#define POWER_CTRL_REBOOT 1

#define COM_META_REQ 0x1
#define COM_META_RES 0x2
#define COM_META_EXT 0x4
#define COM_META_ALL 0x7

#define ERR_RD_RET_DFL 0
#define ERR_WR_RET_DFL 0

#define ACCESS_WR 1
#define ACCESS_RD 0

#define PROC_STA_RDY 1

static char * ctx_sta_string[] = {
	"INIT",
	"NOPC",
	"RADY",
	"RREQ",
	"PROC",
	"SRES",
	"RFNI",
};

enum ctx_sta_stages {
	STA_INIT = 0,
	STA_NO_PROC,
	STA_READY,
	STA_RECV_REQ,
	STA_WAIT_PROC,
	STA_SEND_RES,
	STA_RECV_FINI,
};

struct lom_mgmt_res_hdr {
	uint8_t  code;
	uint16_t meta;
} __attribute__((__packed__));

struct lom_mgmt_res_proc {
	uint16_t size;
	struct {
		uint8_t  code;
		uint16_t meta;
	} __attribute__((__packed__));
};


struct lom_mgmt_i2c_context {
	bool first_write;

	uint8_t state;
	uint8_t state_last; /* only be used in write(recv request) work flow */

	uint8_t first_data_access; /* control executing of prepare_data_access() */

	int access_addr;
	int access_mode; /* read or write */

	/*
	 * Request's meta data
	 */
	uint16_t req_idx;
	uint8_t  req_func_last; /* used for func FINI */

	/*
	 * Response's meta data
	 */
	uint16_t res_idx;
	uint16_t res_idx_last;

	/*
	 * App's data
	 */
	struct lom_mgmt_i2c_callbacks *cb;

	uint8_t rdy_sigs;

	uint8_t res_ready; /* exchange info between EC-I2C and EC-LOM proc */

	struct lom_mgmt_res_proc res_proc; /* used in STA_WAIT_PROC */
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

static void lom_mgmt_tgt_deliver_request(struct lom_mgmt_i2c_context *ctx);

static void set_ctx_response_meta(struct lom_mgmt_i2c_context* ctx,
	uint8_t code, uint16_t dat_size, uint8_t flag, uint8_t state);

static inline int set_response_info(struct lom_mgmt_res* res,
	uint8_t code, uint16_t dat_size, uint8_t flag)
{
	if (dat_size > RES_DLEN_MAX) {
		return -1;
	}

	res->code = code;
	res->meta = htons((dat_size & 0xFFF) | ((uint16_t)(flag & 0xF) << 12));

	if (code == EC_RET_OK)
		res->size = dat_size + RES_HEAD_LEN;
	else
		res->size = RES_HEAD_LEN;

	return 0;
}

int lom_mgmt_i2c_set_callbacks(const struct device *dev, struct lom_mgmt_i2c_callbacks *cb)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_DEV(dev);

	LOG_INF("LOM_MGMT_PROC Registered");

	ctx->cb = cb;
	if (!ctx->cb) {
		LOG_ERR("LOM_MGMT_PROC callback is NULL");
		return -1;
	}

	set_ctx_response_meta(ctx, EC_RET_READY, 0, 0, STA_READY);

	return 0;
}

int lom_mgmt_i2c_set_avail_res(const struct device *dev, uint8_t mask, uint8_t avail)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_DEV(dev);

	ctx->rdy_sigs = (ctx->rdy_sigs & ~mask) | (avail & mask);

	return 0;
}

int lom_mgmt_i2c_response_ready(const struct device *dev,
	uint8_t code, uint16_t dat_size, uint8_t flag)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_DEV(dev);

	if (ctx->state != STA_WAIT_PROC) {
		LOG_DBG_STA("(S) !ERR!: [%s]", ctx_sta_string[ctx->state]);
		return 0;
	}

	set_ctx_response_meta(ctx, code, dat_size, flag, STA_SEND_RES);
	ctx->res_ready = 1;

	LOG_DBG_STA("(S) F(%d) Response Ready(sz:%d)", ctx->req_func_last, ctx->res.size);
	//LOG_INF("(S) F(%d) Response Ready %d", ctx->req_func_last, ctx->res.size);

	LOG_DBG_STA("(S) [%s] -> [%s]",
		ctx_sta_string[STA_WAIT_PROC], ctx_sta_string[ctx->state]);

	return 0;
}

static inline void ctx_meta_data_reset(struct lom_mgmt_i2c_context *ctx, int meta_sel)
{
	if (meta_sel & COM_META_EXT) {
		ctx->req_func_last = 0;
		ctx->access_addr = -1;
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

static void state_recover(struct lom_mgmt_i2c_context * ctx)
{
	LOG_DBG_STA("(S) Recover: [%s] -> [%s]",
		ctx_sta_string[ctx->state], ctx_sta_string[ctx->state_last]);

	ctx->state = ctx->state_last;
}

static int lom_mgmt_tgt_wr_req(struct i2c_target_config *config)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_CFG(config);

	ctx->first_write = true;

	LOG_DBG_I2C_DAT("first_write=%d", ctx->first_write);

	return 0;
}

static inline int need_timestamp(uint16_t res_meta)
{
	return (ntohs(res_meta) >> 12) & RES_META_F_TIMESTAMP
}

static inline void set_timestamp(uint8_t *buf)
{
	uint32_t ts = k_uptime_get_32();

	buf[0] = (ts >> 24) & 0xFF;
	buf[1] = (ts >> 16) & 0xFF;
	buf[2] = (ts >>  8) & 0xFF;
	buf[3] = (ts >>  0) & 0xFF;
}

static int prepare_data_access(struct lom_mgmt_i2c_context *ctx)
{
	if (ctx->first_data_access == 0)
		return 0;

	ctx->first_data_access = 0;

	/* Data Write */
	if (ctx->access_mode == ACCESS_WR) {
		if (ctx->access_addr == 0) {
			goto error_quit;
		}

		if (ctx->access_addr == I2C_REQ_ADDR) {
			if (ctx->state == STA_READY) {
				ctx->state_last = ctx->state;
				ctx->state = STA_RECV_REQ;
				LOG_DBG_STA("(S) [Wr] [%s] -> [%s]",
					ctx_sta_string[ctx->state_last],
					ctx_sta_string[ctx->state]);
				return 0;
			}
		}
		else if (ctx->access_addr == I2C_FNI_ADDR) {
			/* prepare for recv FINI command */
			ctx_meta_data_reset(ctx, COM_META_REQ);
			ctx->state_last = ctx->state;
			ctx->state = STA_RECV_FINI;
			LOG_DBG_STA("(S) [Wr] [%s] -> [%s]",
				ctx_sta_string[ctx->state_last], ctx_sta_string[ctx->state]);

			return 0;
		}
	}
	/* Data Read */
	else {
		if (ctx->access_addr == 0) {
			return 0;
		}
		else if (ctx->access_addr == COMM_RES_STAT) {
			ctx->res_idx = 0;
		}
		else {
#ifndef DEBUG_TEST
			if (ctx->state != STA_SEND_RES) {
				goto error_quit;
			}
#endif

			uint16_t idx = RES_HEAD_LEN + (ctx->access_addr - 2) * 32;
#ifndef DEBUG_TEST
			if (idx >= RES_BUFF_SIZE || idx >= ctx->res.size) {
#else
			if (idx >= RES_BUFF_SIZE) {
#endif
				LOG_DBG("read req exceeds the limit");
				goto error_quit;
			}

			ctx->res_idx = idx;

			if (ctx->res_ready) {
				if (need_timestamp(ctx->res.meta)) {
					set_timestamp(ctx->res.data);
				}

				ctx->res_ready = 0;
			}
		}

		LOG_DBG_STA("(S) Move to RES[%d/%d]", ctx->res_idx, ctx->res.size-1);

		return 0;
	}

error_quit:
	LOG_DBG_STA("(S) !ERR!: STA[%s], %s @ COMM[0x%02x]",
		ctx_sta_string[ctx->state], ctx->access_mode == ACCESS_WR ? "Wr" : "Rd",
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

		LOG_DBG_STA("(+) New Access @ COMM[0x%02x]", ctx->access_addr);
	}
	else {  /* this should be the request's data */
		ctx->access_mode = ACCESS_WR;

		if (prepare_data_access(ctx) < 0) {
			return 0;
		}

		if (ctx->state != STA_RECV_REQ && ctx->state != STA_RECV_FINI) {
			return 0;
		}

		ctx->req.buf[ctx->req_idx] = val;

		LOG_DBG_I2C_DAT("COMM[0x%02x] REQ[%d] <= 0x%02x",
			ctx->access_addr, ctx->req_idx, val);

		ctx->req_idx = (ctx->req_idx + 1) % REQ_BUFF_SIZE;
	}

	return 0;
}

/*
 * This function's return will be ignored by upper framework
 */
static int lom_mgmt_tgt_stop(struct i2c_target_config *config)
{
	struct lom_mgmt_i2c_context * ctx = LOM_MGMT_CTX_FROM_CFG(config);

	LOG_DBG_I2C_DAT(">> [%s]", ctx->access_mode == ACCESS_WR ? "WR" : "RD");

	if (ctx->access_addr && ctx->access_mode == ACCESS_WR) {
		ctx->req.size = ctx->req_idx;
		lom_mgmt_tgt_deliver_request(ctx);
	}

	ctx_meta_data_reset(ctx, COM_META_REQ);

	ctx->access_addr = -1;
	ctx->access_mode = ACCESS_RD;

	return 0;
}

/*
 * i2c_smbus_read_quick() will reach here directly
 */
static int lom_mgmt_tgt_rd_req(struct i2c_target_config *config, uint8_t *val)
{
	struct lom_mgmt_i2c_context * ctx = LOM_MGMT_CTX_FROM_CFG(config);

	LOG_DBG(">>");

	ctx->access_mode = ACCESS_RD;

	if (prepare_data_access(ctx) < 0) {
		return 0;
	}

	if (ctx->access_addr <= 0) {
		LOG_DBG(">> Set rdy_sigs 0x%02x", ctx->rdy_sigs);
		*val = ctx->rdy_sigs;
		return 0;
	}

	assert(ctx->res.size && ctx->res_idx != ctx->res.size);

	*val = ctx->res.buf[ctx->res_idx];

#ifdef DEBUG_TEST
	if (ctx->res_idx >= 7 && (*val != (ctx->res_idx - 3) % 256))
		LOG_DBG(">> OOPS 0");
#endif

	LOG_DBG_I2C_DAT("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);
	//LOG_WRN("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);

	return 0;
}

/*
 * Only transaction via lom_mgmt_tgt_wr_rcv() will reach here
 */
static int lom_mgmt_tgt_rd_rcv(struct i2c_target_config *config, uint8_t *val)
{
	struct lom_mgmt_i2c_context *ctx = LOM_MGMT_CTX_FROM_CFG(config);

	LOG_DBG(">>");

	if (ctx->access_addr <= 0) {
		return 0;
	}

	if (ctx->res.size == 0) {
		LOG_ERR("read done, exceeds the limit %d", ctx->res.size);
		return 0;
	}

	ctx->res_idx = (ctx->res_idx + 1) % ctx->res.size;

	*val = ctx->res.buf[ctx->res_idx];

#ifdef DEBUG_TEST
    if (ctx->res_idx >= 7 && (*val != (ctx->res_idx - 3) % 256))
        LOG_DBG(">> OOPS 1");
#endif

	LOG_DBG_I2C_DAT("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);
	//LOG_WRN("COMM[0x%02x] RES[%d] => 0x%02x", ctx->access_addr, ctx->res_idx, *val);

	return 0;
}

static void set_ctx_response_meta(struct lom_mgmt_i2c_context* ctx,
	uint8_t code, uint16_t dat_size, uint8_t flag, uint8_t state)
{
	set_response_info(&ctx->res, code, dat_size, flag);

	LOG_DBG_STA("(S) [%s] -> [%s]", ctx_sta_string[ctx->state], ctx_sta_string[state]);

	ctx->state = state;
	ctx->res_idx = 0;
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
		LOG_DBG("invalid csum 0x%02x, should be 0x%02x", csum_org, csum);
		return false;
	}

	return true;
}

static void lom_mgmt_tgt_do_fini(struct lom_mgmt_i2c_context *ctx)
{
	LOG_DBG("FINI: Last Func<%02d> STA[%s] RES[(FYI)%4d/%-4d]", ctx->req_func_last,
		ctx_sta_string[ctx->state_last], ctx->res_idx, ctx->res.size-1);

	ctx_meta_data_reset(ctx, COM_META_ALL);

	set_ctx_response_meta(ctx, EC_RET_READY, 0, 0, STA_READY);

	ctx->req_func_last = 0;

	LOG_DBG("LOM MGMT RST!");
}


/*
 *  ALERT: this will delay the I2C bus.
 */
static void lom_mgmt_tgt_do_testl2(struct lom_mgmt_i2c_context *ctx)
{
	uint16_t tst_size = ((uint16_t)ctx->req.data[0] << 8) | ctx->req.data[1]; /* test size */

	LOG_DBG("test_l2 size %d\n", tst_size);

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
	LOG_DBG(">> Input(size %d): seed 0x%02x, func %d, dlen %d", ctx->req.size,
		ctx->req.seed, ctx->req.func, ctx->req.dlen);

	/*
	  the data passes all the followed sanity checking, we think this is a request
	*/
	if (ctx->req.size < REQ_HEAD_LEN || ctx->req.dlen > REQ_DLEN_MAX ||
		(ctx->req.dlen != ctx->req.size - REQ_HEAD_LEN)) {
		LOG_ERR("%s: %u", err_string[EC_RET_ERR_INV_SIZE], ctx->req.size);
		goto error_quit;
	}

	if (check_csum(ctx) == false) {
		LOG_ERR("%s", err_string[EC_RET_ERR_INV_CSUM]);
		goto error_quit;
	}

#if 0
	if (ctx->req.func > FUNC_LAST || ctx->req.func < FUNC_FIRST) {
		LOG_ERR("%s: %d", err_string[EC_RET_ERR_INV_FUNC], ctx->req.func);
		goto error_quit;
	}

	if (ctx->req.seed == 0) {
		LOG_ERR("%s", err_string[EC_RET_ERR_INV_SEED]);
		goto error_quit;
	}
#endif

	if (ctx->req.func == FUNC_FINI) {
		lom_mgmt_tgt_do_fini(ctx);
		return;
	}

	ctx->res_idx = 0;
	ctx->req_func_last = ctx->req.func;
	ctx->res_ready = 0;

	if (ctx->req.func == FUNC_TEST_L2) {
		lom_mgmt_tgt_do_testl2(ctx);
	}
#ifdef LOM_MGMT_DBG
	else if (ctx->req.func == FUNC_DEBUG) {
	}
#endif
	else {
		if (ctx->cb == NULL) {
			set_ctx_response_meta(ctx, EC_RET_NO_PROC, 0, 0, STA_SEND_RES);
			return;
		}

		set_ctx_response_meta(ctx, EC_RET_RETRY, 0, 0, STA_WAIT_PROC);

		ctx->cb->send_request(&ctx->req, &ctx->res);

		LOG_DBG(">> Send request to app");
	}

	return;

error_quit:
	LOG_HEXDUMP_ERR(ctx->req.buf, REQ_HEAD_LEN, "ReqHdr");
	state_recover(ctx);
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

	LOG_INF("init at addr 0x%02x", cfg->bus.addr);

	data->config.address = cfg->bus.addr;
	data->config.callbacks = &lom_mgmt_callbacks;

	data->ctx.rdy_sigs = 0;
	data->ctx.access_addr = -1;

	set_ctx_response_meta(&data->ctx, EC_RET_NO_PROC, 0, 0, STA_NO_PROC);

	if (lom_mgmt_target_register(dev) < 0)
		LOG_ERR("%s, Register failed", __func__);

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
