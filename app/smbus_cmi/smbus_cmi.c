/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include "acpi_region.h"
#include "smbus_cmi.h"
#include "smbus_cmi_region.h"

LOG_MODULE_REGISTER(smbus_cmi, CONFIG_SMBUS_CMI_LOG_LEVEL);

/* The board's silicom,smbus-cmi node - see
 * out_of_tree_boards/boards/arm/mec172x_adl_n_cp/dts/bindings/smbus/
 * silicom,smbus-cmi.yaml. Its 'i2c-bus' phandle names the local I2C/SMBus
 * controller requests are relayed onto; the host addresses this interface
 * through g_acpi_tbl.acpi_smb_buffer on the existing ACPI_EC_0 channel
 * app/smchost already owns, not a dedicated ACPI EC channel.
 */
#define CMI_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(silicom_smbus_cmi)

BUILD_ASSERT(DT_NODE_EXISTS(CMI_NODE),
	     "CONFIG_SMBUS_CMI requires a 'silicom,smbus-cmi' devicetree node "
	     "(i2c-bus phandle)");

static const struct device *cmi_i2c_bus = DEVICE_DT_GET(DT_PHANDLE(CMI_NODE, i2c_bus));

BUILD_ASSERT(sizeof(struct smbus_cmi_region) <=
	     sizeof(((struct acpi_tbl *)0)->acpi_smb_buffer),
	     "struct smbus_cmi_region must fit within acpi_tbl's acpi_smb_buffer[40]");

/* Overlaid directly onto app/smchost's g_acpi_tbl.acpi_smb_buffer - the host
 * addresses this region through the same indexed ACPI_EC_0 reads/writes
 * (and gets the same automatic generate_sci() on each one) as every other
 * field in g_acpi_tbl. No separate polling thread or SCI GPIO needed.
 */
#define g_cmi_region (*(struct smbus_cmi_region *)(g_acpi_tbl.acpi_smb_buffer))

static void smbus_cmi_execute(void)
{
	uint16_t addr = g_cmi_region.addr;
	uint8_t bcnt = MIN(g_cmi_region.bcnt, sizeof(g_cmi_region.data));
	int ret;

	if (!device_is_ready(cmi_i2c_bus)) {
		LOG_ERR("SMBus CMI I2C bus not ready");
		g_cmi_region.stat = SMBUS_CMI_STAT_UNKNOWN_FAILURE;
		g_cmi_region.prtcl = 0;
		return;
	}

	switch (g_cmi_region.prtcl) {
	case SMBUS_CMI_PRTCL_WRITE_QUICK:
	case SMBUS_CMI_PRTCL_READ_QUICK: {
		struct i2c_msg msg = {
			.buf = NULL,
			.len = 0,
			.flags = (g_cmi_region.prtcl == SMBUS_CMI_PRTCL_READ_QUICK ?
				  I2C_MSG_READ : I2C_MSG_WRITE) | I2C_MSG_STOP,
		};
		ret = i2c_transfer(cmi_i2c_bus, &msg, 1, addr);
		break;
	}
	case SMBUS_CMI_PRTCL_SEND_BYTE:
		ret = i2c_write(cmi_i2c_bus, &g_cmi_region.cmd, 1, addr);
		break;
	case SMBUS_CMI_PRTCL_RECEIVE_BYTE:
		ret = i2c_read(cmi_i2c_bus, &g_cmi_region.data[0], 1, addr);
		break;
	case SMBUS_CMI_PRTCL_WRITE_BYTE:
		ret = i2c_burst_write(cmi_i2c_bus, addr, g_cmi_region.cmd,
				       &g_cmi_region.data[0], 1);
		break;
	case SMBUS_CMI_PRTCL_READ_BYTE:
		ret = i2c_burst_read(cmi_i2c_bus, addr, g_cmi_region.cmd,
				      &g_cmi_region.data[0], 1);
		break;
	case SMBUS_CMI_PRTCL_WRITE_WORD:
		ret = i2c_burst_write(cmi_i2c_bus, addr, g_cmi_region.cmd,
				       &g_cmi_region.data[0], 2);
		break;
	case SMBUS_CMI_PRTCL_READ_WORD:
		ret = i2c_burst_read(cmi_i2c_bus, addr, g_cmi_region.cmd,
				      &g_cmi_region.data[0], 2);
		break;
	case SMBUS_CMI_PRTCL_WRITE_BLOCK:
		ret = i2c_burst_write(cmi_i2c_bus, addr, g_cmi_region.cmd,
				       &g_cmi_region.data[0], bcnt);
		break;
	case SMBUS_CMI_PRTCL_READ_BLOCK:
		ret = i2c_burst_read(cmi_i2c_bus, addr, g_cmi_region.cmd,
				      &g_cmi_region.data[0], bcnt);
		break;
	case SMBUS_CMI_PRTCL_PROCESS_CALL: {
		uint8_t rx[2];

		ret = i2c_write_read(cmi_i2c_bus, addr, &g_cmi_region.data[0], 2, rx, 2);
		if (!ret)
			memcpy(g_cmi_region.data, rx, sizeof(rx));
		break;
	}
	case SMBUS_CMI_PRTCL_BLOCK_PROCESS_CALL: {
		uint8_t rx[sizeof(g_cmi_region.data)];

		ret = i2c_write_read(cmi_i2c_bus, addr, &g_cmi_region.data[0], bcnt, rx, bcnt);
		if (!ret)
			memcpy(g_cmi_region.data, rx, bcnt);
		break;
	}
	default:
		LOG_WRN("SMBus CMI unsupported protocol: %02x", g_cmi_region.prtcl);
		g_cmi_region.stat = SMBUS_CMI_STAT_UNSUPPORTED_PROTOCOL;
		g_cmi_region.prtcl = 0;
		return;
	}

	if (ret) {
		LOG_WRN("SMBus CMI protocol %02x failed: %d", g_cmi_region.prtcl, ret);
		g_cmi_region.stat = (ret == -ETIMEDOUT) ? SMBUS_CMI_STAT_TIMEOUT :
				     SMBUS_CMI_STAT_ADDR_NOT_ACKED;
	} else {
		g_cmi_region.stat = SMBUS_CMI_STAT_OK;
	}

	/* Per ACPI's EC-SMBus protocol (and the CMI spec's identical sample
	 * ASL SWTC wait-method), completion is signaled by the controller
	 * clearing PRTCL back to 0 after STAT is updated - not by STAT alone.
	 */
	g_cmi_region.prtcl = 0;
}

void smbus_cmi_handle_ec_write(uint8_t acpi_idx)
{
	if (acpi_idx != offsetof(struct acpi_tbl, acpi_smb_buffer) +
			offsetof(struct smbus_cmi_region, prtcl)) {
		return;
	}

	smbus_cmi_execute();
}
