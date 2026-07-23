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
static K_MUTEX_DEFINE(g_sys_ids_mutex);

/* LOM IP address (network order). family == LOM_IP_FAMILY_NONE means the
 * LOM has not reported an address yet.
 */
static struct {
	uint8_t family;                   /* LOM_IP_FAMILY_V4 / _V6 / _NONE */
	uint8_t prefix;                   /* CIDR netmask bits */
	uint8_t addr[LOM_IPV6_ADDR_LEN];  /* octets, network order */
} g_lom_ip = { .family = LOM_IP_FAMILY_NONE };
K_MUTEX_DEFINE(lom_ip_mutex);

uint16_t get_platform_id(void)
{
	return plat_data;
}

uint8_t get_bom_id (void)
{
    uint8_t bom_id = 0;
    if (k_mutex_lock(&g_sys_ids_mutex, K_MSEC(500)) != 0) {
        LOG_ERR("%s: mutex timeout", __func__);
        return 0;
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
        return 0;
    }
    brd_id = g_sys_ids.board_id;
    k_mutex_unlock(&g_sys_ids_mutex);
    return brd_id;
}

void get_sbl_version(struct sbl_version *sbl)
{
    /*
      image_id; (8 bytes array)
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

/*
 * Store the LOM IP address reported over the (separately handled) LOM
 * management link. Writer runs in the lom_mgmt thread context while the
 * reader runs in the smchost thread, so guard the copy with a mutex.
 *
 * family selects IPv4 (4 octets) or IPv6 (16 octets); len must match the
 * family. addr holds the octets in network order (addr[0] = most
 * significant). prefix is the subnet mask as a CIDR bit count.
 */
void set_lom_ip(uint8_t family, uint8_t prefix, const uint8_t *addr, uint8_t len)
{
    uint8_t expected;

    switch (family) {
    case LOM_IP_FAMILY_V4:
        expected = LOM_IPV4_ADDR_LEN;
        break;
    case LOM_IP_FAMILY_V6:
        expected = LOM_IPV6_ADDR_LEN;
        break;
    default:
        LOG_WRN("%s: bad LOM IP family 0x%02X", __func__, family);
        return;
    }

    if (len != expected) {
        LOG_WRN("%s: LOM IP len %u != %u for family 0x%02X",
                __func__, len, expected, family);
        return;
    }

    k_mutex_lock(&lom_ip_mutex, K_FOREVER);
    g_lom_ip.family = family;
    g_lom_ip.prefix = prefix;
    memset(g_lom_ip.addr, 0, sizeof(g_lom_ip.addr));
    memcpy(g_lom_ip.addr, addr, len);
    k_mutex_unlock(&lom_ip_mutex);
}

/*
 * Build the host response into out[] (must be >= LOM_IP_RESP_MAX bytes):
 * [family][prefix][address octets]. Returns the number of bytes written.
 * When no address has been reported, only family + prefix are written
 * (family == LOM_IP_FAMILY_NONE).
 */
uint8_t get_lom_ip_resp(uint8_t *out)
{
    uint8_t len = 0;
    uint8_t addr_len;

    k_mutex_lock(&lom_ip_mutex, K_FOREVER);

    switch (g_lom_ip.family) {
    case LOM_IP_FAMILY_V4:
        addr_len = LOM_IPV4_ADDR_LEN;
        break;
    case LOM_IP_FAMILY_V6:
        addr_len = LOM_IPV6_ADDR_LEN;
        break;
    default:
        addr_len = 0;
        break;
    }

    out[len++] = g_lom_ip.family;
    out[len++] = g_lom_ip.prefix;
    memcpy(&out[len], g_lom_ip.addr, addr_len);
    len += addr_len;

    k_mutex_unlock(&lom_ip_mutex);

    return len;
}
