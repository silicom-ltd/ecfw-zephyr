/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EC_VGPIO_REGION_H__
#define __EC_VGPIO_REGION_H__

#include <zephyr/kernel.h>

/* Maximum number of EC-owned GPIO lines this module can expose, bounded by
 * the 8-bit GPIO ID / describe-index fields below (and the devicetree child
 * node count via a BUILD_ASSERT in ec_vgpio.c).
 */
#define EC_VGPIO_MAX_LINES	32

/** Max line name length, matching the 16-byte vgnm field (15 chars + NUL). */
#define EC_VGPIO_MAX_NAME	15

/* ACPI mailbox region for the EC Virtual GPIO Proxy, overlaid directly onto
 * app/smchost's g_acpi_tbl.ec_vgpio_buffer (reclaimed dead space in the
 * existing ACPI_EC_0 channel - see acpi_region.h). ASL sets VGID/VGVL/VGFL
 * as needed for the command, then writes VGCM last to arm and fire it.
 *
 * Commands (VGCM):
 *   1 set            in : VGID = line ID, VGVL = value (0/1)
 *   2 get             out: VGVL = value
 *   3 line count       out: VGVL = number of lines
 *   4 describe index  in : VGID = index; out: VGVL = line ID, VGFL =
 *                      capability flags, VGNM = line name
 *   5 configure IRQ   in : VGID = line ID, VGVL = IRQ config byte
 *   6 pop one event    out: VGVL = line ID, VGFL = event flags
 * VGST reports the result of the last command (see enum ec_vgpio_status).
 * EC clears VGCM to 0 once the command completes.
 */
struct ec_vgpio_region {
	uint8_t vgid;		/* [0x00] line ID (set/get/IRQ cfg) or index (describe) */
	uint8_t vgvl;		/* [0x01] value / IRQ config in; value / count / ID out */
	uint8_t vgcm;		/* [0x02] command; write triggers it */
	uint8_t vgst;		/* [0x03] status (see enum ec_vgpio_status) */
	uint8_t vgfl;		/* [0x04] IRQ config in (cmd 5); flags out (cmd 4/6) */
	uint8_t vgnm[16];	/* [0x05-0x14] line name, ASCII NUL-terminated (cmd 4) */
} __attribute__((__packed__));

/** Max byte offset into struct ec_vgpio_region (sizeof - 1). */
#define EC_VGPIO_MAX_INDEX	(sizeof(struct ec_vgpio_region) - 1)

/* Mailbox commands (VGCM). */
enum ec_vgpio_cmd {
	EC_VGPIO_CMD_SET		= 1,
	EC_VGPIO_CMD_GET		= 2,
	EC_VGPIO_CMD_LINE_COUNT		= 3,
	EC_VGPIO_CMD_DESCRIBE		= 4,
	EC_VGPIO_CMD_IRQ_CFG		= 5,
	EC_VGPIO_CMD_POP_EVENT		= 6,
};

/* Mailbox status codes (VGST). */
enum ec_vgpio_status {
	EC_VGPIO_ST_OK			= 0,
	EC_VGPIO_ST_DENIED		= 1,
	EC_VGPIO_ST_BAD_ID		= 2,
	EC_VGPIO_ST_BAD_VALUE		= 3,
	EC_VGPIO_ST_QUEUE_EMPTY		= 4,
};

/* Capability flags reported by cmd 4 (VGFL out). */
#define EC_VGPIO_FL_INPUT	BIT(0)
#define EC_VGPIO_FL_OUTPUT	BIT(1)
#define EC_VGPIO_FL_HOST_DRIVE	BIT(2)
#define EC_VGPIO_FL_ACTIVE_LOW	BIT(3)
#define EC_VGPIO_FL_IRQ		BIT(4)

/* IRQ config byte (cmd 5, VGVL in). */
#define EC_VGPIO_IRQ_EN		BIT(0)
#define EC_VGPIO_IRQ_TRIG_MASK	GENMASK(2, 1)
#define EC_VGPIO_IRQ_RISING	(1 << 1)
#define EC_VGPIO_IRQ_FALLING	(2 << 1)
#define EC_VGPIO_IRQ_BOTH	(3 << 1)

/* Event flags (cmd 6, VGFL out). */
#define EC_VGPIO_EV_LEVEL	BIT(0)
#define EC_VGPIO_EV_OVERFLOW	BIT(7)

#endif	/* __EC_VGPIO_REGION_H__ */
