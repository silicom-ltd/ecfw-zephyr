/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LOM_PWRCYCLE_H__
#define __LOM_PWRCYCLE_H__

/**
 * @brief Request a LOM-initiated power cycle of the host.
 *
 * Forces the host down, confirms it reached S5, removes switch card power for
 * the configured dwell, then powers the host back on. Switch card power is
 * restored by the power sequencer during power_on(), which keeps the
 * CPU-then-switch ordering.
 *
 * Non-blocking: signals the power cycle thread and returns immediately. A
 * request arriving while a cycle is already running is ignored.
 *
 * The signature is void(void) so it can be installed directly in the
 * LOM-MGMT power control dispatch table.
 */
void lom_pwrcycle_start(void);

#endif /* __LOM_PWRCYCLE_H__ */
