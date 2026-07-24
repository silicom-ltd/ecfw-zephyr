/*
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POSTCODE_H__
#define __POSTCODE_H__

int postcode_add(uint16_t code, uint16_t boot_seq);
int postcode_get(uint8_t *buf, uint16_t buf_size, uint16_t *ret_size);
int postcode_count(void);

#endif /* __POSTCODE_H__ */
