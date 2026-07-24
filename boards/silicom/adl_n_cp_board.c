/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "board_config.h"
#include "board.h"
#include "errcodes.h"
#include "pwrplane.h"
#include "flashhdr.h"

LOG_MODULE_REGISTER(board, CONFIG_BOARD_LOG_LEVEL);

static uint16_t plat_data = 0x2400;
static struct sys_ids g_sys_ids;
static K_MUTEX_DEFINE(g_sys_ids_mutex);

uint16_t get_platform_id(void)
{
	return plat_data;
}

uint8_t get_bom_id (void)
{
    uint8_t bom_id = 0;
    if (k_mutex_lock(&g_sys_ids_mutex, K_MSEC(500)) != 0) {
        LOG_ERR("%s: mutex timeout", __func__);
        return bom_id;
    }
    bom_id = g_sys_ids.bom_id;
    k_mutex_unlock(&g_sys_ids_mutex);
    return bom_id;
}

uint8_t get_board_id (void)
{
    uint8_t brd_id = 0;
    if (k_mutex_lock(&g_sys_ids_mutex, K_MSEC(500)) != 0) {
        LOG_ERR("%s: mutex timeout", __func__);
        return brd_id;
    }
    brd_id = g_sys_ids.board_id;
    k_mutex_unlock(&g_sys_ids_mutex);
    return brd_id;
}

void get_sbl_version(struct sbl_version *sbl)
{
    /*
      image_id[8];
      major_version;
      minor_version;
      build_number_high;
      build_number_low;
      flags;
        //flags: [3:0]reserved, [4]image_arch,
        //       [5]bld_debug,  [6]fsp_debug,
        //       [7]dirty
    */

    if (k_mutex_lock(&g_sys_ids_mutex, K_MSEC(500)) != 0) {
        LOG_ERR("%s: mutex timeout", __func__);
        return;
    }
    *sbl = g_sys_ids.sbl;
    k_mutex_unlock(&g_sys_ids_mutex);
}

void set_sys_ids(const uint8_t *pdata, uint8_t len)
{
    if (k_mutex_lock(&g_sys_ids_mutex, K_MSEC(500)) != 0) {
        LOG_ERR("%s: mutex timeout", __func__);
        return;
    }
    memcpy(&g_sys_ids, pdata, len);
    k_mutex_unlock(&g_sys_ids_mutex);
}
