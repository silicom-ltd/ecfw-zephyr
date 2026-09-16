/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SMBUS_CMI_REGION_H__
#define __SMBUS_CMI_REGION_H__

#include <zephyr/kernel.h>

/* Byte-addressed ACPI SMBus Control Method Interface region, matching the
 * common/generic SMBus-CMI EmbeddedControl OperationRegion layout: ASL sets
 * ADDR/CMD/BCNT/DATA, then writes PRTCL last to arm and fire the
 * transaction, then polls STAT until the busy bit clears.
 *
 * This region is overlaid directly onto app/smchost's g_acpi_tbl.acpi_smb_buffer
 * (the main ACPI_EC_0 channel's existing 40-byte SMBus scratch area), not a
 * separate ACPI EC channel - see smbus_cmi.c.
 *
 * This is the widely-used convention, not a value taken from this repo -
 * confirm every offset and protocol/status code below against the actual
 * BIOS ASL this pairs with before relying on it in production.
 */
struct smbus_cmi_region {
	uint8_t prtcl;		/* [0x00] protocol code; write triggers txn */
	uint8_t stat;		/* [0x01] status; bit7 = busy while in progress */
	uint8_t addr;		/* [0x02] target 7-bit SMBus address */
	uint8_t cmd;		/* [0x03] command/register byte */
	uint8_t data[32];	/* [0x04-0x23] data block */
	uint8_t bcnt;		/* [0x24] block byte count */
	uint8_t aadr;		/* [0x25] SMBus Alert address */
	uint8_t alrm_data0;	/* [0x26] SMBus Alert data byte 0 (optional) */
	uint8_t alrm_data1;	/* [0x27] SMBus Alert data byte 1 (optional) */
} __attribute__((__packed__));

/** Max byte offset into struct smbus_cmi_region (sizeof - 1). */
#define SMBUS_CMI_MAX_INDEX	(sizeof(struct smbus_cmi_region) - 1)

/* SMBus CMI protocol codes (PRTCL), common/generic convention. */
enum smbus_cmi_protocol {
	SMBUS_CMI_PRTCL_WRITE_QUICK		= 0x02,
	SMBUS_CMI_PRTCL_READ_QUICK		= 0x03,
	SMBUS_CMI_PRTCL_SEND_BYTE		= 0x04,
	SMBUS_CMI_PRTCL_RECEIVE_BYTE		= 0x05,
	SMBUS_CMI_PRTCL_WRITE_BYTE		= 0x06,
	SMBUS_CMI_PRTCL_READ_BYTE		= 0x07,
	SMBUS_CMI_PRTCL_WRITE_WORD		= 0x08,
	SMBUS_CMI_PRTCL_READ_WORD		= 0x09,
	SMBUS_CMI_PRTCL_WRITE_BLOCK		= 0x0A,
	SMBUS_CMI_PRTCL_READ_BLOCK		= 0x0B,
	SMBUS_CMI_PRTCL_PROCESS_CALL		= 0x0C,
	SMBUS_CMI_PRTCL_BLOCK_PROCESS_CALL	= 0x0D,
};

/* SMBus CMI status codes (STAT low bits; bit7 is the busy flag). */
enum smbus_cmi_status {
	SMBUS_CMI_STAT_OK			= 0x00,
	SMBUS_CMI_STAT_UNSUPPORTED_PROTOCOL	= 0x01,
	SMBUS_CMI_STAT_BUS_ERROR		= 0x02,
	SMBUS_CMI_STAT_DEVICE_ERROR		= 0x03,
	SMBUS_CMI_STAT_TIMEOUT			= 0x04,
	SMBUS_CMI_STAT_UNKNOWN_ERROR		= 0x07,
};
#define SMBUS_CMI_STAT_BUSY_BIT		BIT(7)

#endif	/* __SMBUS_CMI_REGION_H__ */
