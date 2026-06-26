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

static uint16_t plat_data = 0x2300;
static struct sys_ids g_sys_ids;

/* LOM IPv4 address (network order). All-0xFF = not yet reported by the LOM. */
static uint8_t g_lom_ip[LOM_IP_ADDR_LEN] = { 0xFF, 0xFF, 0xFF, 0xFF };

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
    memcpy(&g_sys_ids, pdata, len);
}

/*
 * Store the LOM IPv4 address reported over the (separately handled) LOM
 * management link. Writer runs in the lom_mgmt thread context while the
 * reader runs in the smchost thread, so guard the copy against tearing.
 */
void set_lom_ip(const uint8_t *addr, uint8_t len)
{
    unsigned int key;

    if (len < LOM_IP_ADDR_LEN) {
        LOG_WRN("%s: short LOM IP length %u", __func__, len);
        return;
    }

    key = irq_lock();
    memcpy(g_lom_ip, addr, LOM_IP_ADDR_LEN);
    irq_unlock(key);
}

/* Copy the 4 IPv4 octets to out[]; all-0xFF if the LOM has not reported one. */
void get_lom_ip_addr(uint8_t *out)
{
    unsigned int key;

    key = irq_lock();
    memcpy(out, g_lom_ip, LOM_IP_ADDR_LEN);
    irq_unlock(key);
}
