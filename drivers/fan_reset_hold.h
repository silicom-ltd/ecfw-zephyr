/*
 * Copyright (c) 2026 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FAN_RESET_HOLD_H__
#define __FAN_RESET_HOLD_H__

#include <stdbool.h>

/** True while any silicom,fan-reset-hold fan controller is still held in reset. */
bool fan_reset_hold_active(void);

#endif /* __FAN_RESET_HOLD_H__ */
