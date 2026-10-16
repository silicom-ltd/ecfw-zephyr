/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __EC_VGPIO_H__
#define __EC_VGPIO_H__

#include <stdint.h>

/**
 * @brief Initialize the EC Virtual GPIO Proxy.
 *
 * Enumerates the devicetree-configured lines, puts every interrupt-capable
 * line into a known (disabled) state, and discards any stale queued events.
 * Call once at board/task init - see app/power_sequencing/pwrplane.c.
 */
void ec_vgpio_init(void);

/**
 * @brief Notify the EC Virtual GPIO Proxy of a completed write into
 * g_acpi_tbl.ec_vgpio_buffer.
 *
 * Called by app/smchost/smchost.c's acpi_write_ec() after every permitted
 * indexed write. Fires the mailbox command when the write landed on the
 * region's 'vgcm' byte; otherwise a no-op.
 *
 * @param acpi_idx absolute byte offset into g_acpi_tbl just written.
 */
void ec_vgpio_handle_ec_write(uint8_t acpi_idx);

#endif	/* __EC_VGPIO_H__ */
