/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POSTCODE_H__
#define __POSTCODE_H__

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

int postcode_add(uint16_t code, uint16_t boot_seq);
int postcode_get(uint8_t *ret, uint16_t *ret_size);

#endif /* __POSTCODE_H__ */
