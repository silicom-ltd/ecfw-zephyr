/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HOST_POWER_DOMAIN_H__
#define __HOST_POWER_DOMAIN_H__

#include <stdbool.h>
#include <zephyr/device.h>

/**
 * @brief Report the host power state to a host power domain.
 *
 * Called by power sequencing, from thread context, whenever the host enters
 * or leaves S0. On power loss every device whose devicetree node has
 * power-domain = <&this_domain> is suspended and sent TURN_OFF, and any
 * outstanding runtime PM claims on the domain are dropped. On power up the
 * devices are sent TURN_ON; devices without runtime PM enabled are also
 * resumed.
 *
 * Until the first call the domain reports the host as off, so
 * pm_device_runtime_get() on a child device fails with -EAGAIN.
 *
 * Repeated calls with an unchanged state are no-ops.
 *
 * @param dev host power domain device.
 * @param powered true if the host is powered (S0), false otherwise.
 *
 * @retval 0 on success.
 * @retval -ENODEV if the domain device is not ready.
 */
int host_power_domain_set(const struct device *dev, bool powered);

/**
 * @brief Check whether a host power domain currently reports the host on.
 *
 * @param dev host power domain device.
 *
 * @retval true if the host is powered.
 */
bool host_power_domain_is_powered(const struct device *dev);

#endif /* __HOST_POWER_DOMAIN_H__ */
