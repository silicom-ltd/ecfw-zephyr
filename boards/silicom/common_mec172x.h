/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2022 Silicom Connectivity Solutions
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include "mec172x_pin.h"

#ifndef __COMMON_MEC172X_H__
#define __COMMON_MEC172X_H__

#define EC_BGPO_PORT		MCHP_GPIO_MAX_PORT

/* Dummy gpio port */
#define EC_DUMMY_GPIO_PORT		0xFU

/* Dummy gpio default low */
#define EC_DUMMY_GPIO_LOW	EC_GPIO_PORT_PIN(EC_DUMMY_GPIO_PORT, 0x00)
/* Dummy gpio default high */
#define EC_DUMMY_GPIO_HIGH	EC_GPIO_PORT_PIN(EC_DUMMY_GPIO_PORT, 0x01)

#endif	/* __COMMON_MEC172X_H__ */
