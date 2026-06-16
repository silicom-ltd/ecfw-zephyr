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

uint16_t get_platform_id(void)
{
	return plat_data;
}

uint8_t get_bom_id (void)
{
    return g_sys_ids.bom_id;
}

uint8_t get_board_id (void)
{
    return g_sys_ids.board_id;
}

struct sbl_version *get_sbl_version(void)
{
    /*
      major_version;
      minor_version;
      build_number_high;
      build_number_low;
      flags;
        //flags: [3:0]reserved, [4]image_arch,
        //       [5]bld_debug,  [6]fsp_debug,
        //       [7]dirty
    */

    return &g_sys_ids.sbl;
}

void set_sys_ids(const uint8_t *pdata, uint8_t len)
{
    uint8_t i = 0;
    uint8_t *ptr = (uint8_t *)&g_sys_ids;

    while (i != len && i < 10)
    {
        ptr[i] = pdata[i];
        i++;
    }
}
