/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __DRIVERS_I2C_TARGET_LOM_MGMT_H__
#define __DRIVERS_I2C_TARGET_LOM_MGMT_H__

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/net/net_ip.h>

#define REQ_DLEN_MAX 28
#define RES_DLEN_MAX 2048

#define REQ_HEAD_LEN 4
#define RES_HEAD_LEN 3

#define REQ_BUFF_SIZE (REQ_DLEN_MAX + REQ_HEAD_LEN) /* Cannot exceed 32 bytes */
#define RES_BUFF_SIZE (RES_DLEN_MAX + RES_HEAD_LEN)

#if REQ_BUFF_SIZE > 32
#error "Request buffer size cannot exceed 32 bytes"
#endif

struct lom_mgmt_req {
	uint16_t size;
	union {
		struct {
			uint8_t seed;
			uint8_t func;
			uint8_t dlen;
			uint8_t csum;
			uint8_t data[];
		} __attribute__((__packed__));

		uint8_t  buf[REQ_BUFF_SIZE];
	};
};

#define RES_META_F_TIMESTAMP 0x08

#define RES_META_DLEN_NTOH(meta) (ntohs(meta) & 0xFFF)
#define RES_META_FLAG_NTOH(meta) (ntohs(meta) >> 12)

struct lom_mgmt_res {
	uint16_t size;
	union {
		struct {
			uint8_t  code;
			uint16_t meta;
			uint8_t  data[];
		} __attribute__((__packed__));

		uint8_t buf[RES_BUFF_SIZE];
	};
};

typedef int (*lom_mgmt_i2c_cb_request_t)(struct lom_mgmt_req *req, struct lom_mgmt_res *res);

typedef void (*lom_mgmt_i2c_cb_cancel_t)(void);

struct lom_mgmt_i2c_callbacks {
	lom_mgmt_i2c_cb_request_t send_request;
	lom_mgmt_i2c_cb_cancel_t  send_cancel;
};

int lom_mgmt_i2c_response_ready(const struct device *dev, uint8_t code,
	uint16_t dlen_or_sys_error, uint8_t flag);
int lom_mgmt_i2c_set_callbacks(const struct device *dev, struct lom_mgmt_i2c_callbacks *cb);
int lom_mgmt_i2c_set_avail_res(const struct device *dev, uint8_t bit, uint8_t val);

#define AVAIL_RES_POSTCODE     7
#define AVAIL_RES_EVENT        6

//#define LOM_MGMT_PROTO_STRESS_TESTING

enum lom_mgmt_msg_func {
	FUNC_INV                  = 0,
	FUNC_FIRST                = 1,
	FUNC_GET_ID               = FUNC_FIRST,
	FUNC_FINI                 , /*02*/
	FUNC_POWER_CTRL           , /*03*/
	FUNC_GET_ACPI_POWER_STATE , /*04*/
	FUNC_GET_SENSORS          , /*05*/
	FUNC_GET_FRU              , /*06*/
	FUNC_GET_FAULT_CODE       , /*07*/
	FUNC_GET_EVENTS           , /*08*/
	FUNC_GET_POSTCODE         , /*09*/
	FUNC_REPORT_LOM_IP        , /*10*/

#ifdef LOM_MGMT_PROTO_STRESS_TESTING
	FUNC_TEST_L3              ,
#endif
	FUNC_COUNT,
};

enum lom_mgmt_power_ctrl_act {
	PWC_UP = 1,
	PWC_SHUTDOWN,
	PWC_HARD_RESET,
	PWC_FORCE_DOWN,
	PWC_POWER_CYCLE,
};

enum lom_mgmt_i2c_stat {
	EC_RET_NO_PROC = 1,
	EC_RET_READY,
	EC_RET_RETRY, /* Data not ready */
	EC_RET_OK,
	EC_RET_ERR_CODE_BASE,
	EC_RET_ERR_INV_SIZE = EC_RET_ERR_CODE_BASE,
	EC_RET_ERR_INV_FUNC,
	EC_RET_ERR_INV_CSUM,
	EC_RET_ERR_NOT_IMPL,
	EC_RET_ERR_IN_PROCESS,
	EC_RET_ERR_FAIL,
};

#endif /* __DRIVERS_I2C_TARGET_LOM_MGMT_H__ */
