/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SMBUS_CMI_H__
#define __SMBUS_CMI_H__

#include <stdint.h>

/**
 * @brief Notify the SMBus CMI module of a completed write into the ACPI
 * EC's acpi_smb_buffer.
 *
 * Called by app/smchost/smchost.c's acpi_write_ec() after every permitted
 * indexed write into g_acpi_tbl. Fires the SMBus transaction when the
 * write landed on the CMI region's 'prtcl' byte; otherwise a no-op.
 *
 * @param acpi_idx absolute byte offset into g_acpi_tbl just written.
 */
void smbus_cmi_handle_ec_write(uint8_t acpi_idx);

#endif	/* __SMBUS_CMI_H__ */
