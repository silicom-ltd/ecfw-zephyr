/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host power domain.
 *
 * A power domain that does not control a rail; it follows the host power
 * state as reported by power sequencing (host_power_domain_set()). Devices
 * that only work while the host is on (PECI, host-side SMBus, ...) point
 * their power-domain property at it, and then:
 *  - pm_device_runtime_get() on them fails with -EAGAIN while the host is off
 *  - they are suspended and sent TURN_OFF when the host leaves S0
 *  - they are sent TURN_ON when the host re-enters S0
 *
 * Unlike Zephyr's power-domain-gpio-monitor, notifications run in thread
 * context, so children may do bus transactions in their PM action handlers,
 * and the domain state is set explicitly by power sequencing rather than
 * waiting for the first edge of a GPIO, so an EC reset while the host is on
 * is handled.
 */

#define DT_DRV_COMPAT silicom_host_power_domain

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/logging/log.h>

#include "host_power_domain.h"

LOG_MODULE_REGISTER(host_power_domain, CONFIG_HOST_POWER_DOMAIN_LOG_LEVEL);

struct host_pd_data {
	/* Serializes host_power_domain_set() callers */
	struct k_mutex lock;
	/* Read by the PM resume path without taking the lock */
	atomic_t powered;
	/* False until power sequencing has reported a state */
	bool synced;
};

/*
 * Lock a device's runtime PM state against concurrent get/put, the same way
 * the runtime PM core does (spinlock for ISR-safe devices, semaphore
 * otherwise).
 */
static k_spinlock_key_t host_pd_child_lock(const struct device *dev)
{
	k_spinlock_key_t key = {0};

	if (atomic_test_bit(&dev->pm_base->flags, PM_DEVICE_FLAG_ISR_SAFE)) {
		key = k_spin_lock(&dev->pm_isr->lock);
	} else {
		(void)k_sem_take(&dev->pm->lock, K_FOREVER);
	}

	return key;
}

static void host_pd_child_unlock(const struct device *dev, k_spinlock_key_t key)
{
	if (atomic_test_bit(&dev->pm_base->flags, PM_DEVICE_FLAG_ISR_SAFE)) {
		k_spin_unlock(&dev->pm_isr->lock, key);
	} else {
		k_sem_give(&dev->pm->lock);
	}
}

/*
 * Settle a pending pm_device_runtime_put_async(). Called with the child's
 * semaphore held; may drop and retake it while the suspend work finishes.
 */
static void host_pd_child_settle(const struct device *dev)
{
	struct pm_device *pm = dev->pm;

	while (pm->base.state == PM_DEVICE_STATE_SUSPENDING) {
		if ((k_work_cancel_delayable(&pm->work) & K_WORK_RUNNING) == 0) {
			/* Cancelled before it ran: the device is still active */
			pm->base.state = PM_DEVICE_STATE_ACTIVE;
			break;
		}

		k_sem_give(&pm->lock);
		(void)k_event_wait(&pm->event,
				   BIT(PM_DEVICE_STATE_ACTIVE) |
				   BIT(PM_DEVICE_STATE_SUSPENDED),
				   true, K_FOREVER);
		(void)k_sem_take(&pm->lock, K_FOREVER);
	}
}

static bool host_pd_is_child(const struct device *dev, const struct device *domain)
{
	return (dev->pm_base != NULL) && (dev->pm_base->domain == domain);
}

static int host_pd_turn_off_child(const struct device *dev, void *context)
{
	const struct device *domain = context;
	k_spinlock_key_t key;
	int ret;

	if (!host_pd_is_child(dev, domain)) {
		return 0;
	}

	key = host_pd_child_lock(dev);

	if (!atomic_test_bit(&dev->pm_base->flags, PM_DEVICE_FLAG_ISR_SAFE)) {
		host_pd_child_settle(dev);
	}

	if (dev->pm_base->state == PM_DEVICE_STATE_ACTIVE) {
		ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
		if (ret < 0) {
			LOG_WRN("%s: suspend failed: %d", dev->name, ret);
		}
	}

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_OFF);
	if ((ret < 0) && (ret != -EALREADY)) {
		LOG_WRN("%s: turn off failed: %d", dev->name, ret);
	}

	/*
	 * The host is gone whether or not the device agreed: drop any
	 * outstanding claims. A later put() from a user that held the device
	 * across the power loss returns -EALREADY.
	 */
	dev->pm_base->usage = 0U;
	atomic_clear_bit(&dev->pm_base->flags, PM_DEVICE_FLAG_PD_CLAIMED);

	host_pd_child_unlock(dev, key);

	return 0;
}

static int host_pd_turn_on_child(const struct device *dev, void *context)
{
	const struct device *domain = context;
	k_spinlock_key_t key;
	int ret;

	if (!host_pd_is_child(dev, domain)) {
		return 0;
	}

	key = host_pd_child_lock(dev);

	ret = pm_device_action_run(dev, PM_DEVICE_ACTION_TURN_ON);
	if ((ret < 0) && (ret != -EALREADY) && (ret != -ENOTSUP)) {
		/* The core flags the device so runtime get() returns -EAGAIN */
		LOG_WRN("%s: turn on failed: %d", dev->name, ret);
	}

	/* Nothing will resume a device that is not under runtime PM */
	if (!atomic_test_bit(&dev->pm_base->flags, PM_DEVICE_FLAG_RUNTIME_ENABLED) &&
	    (dev->pm_base->state == PM_DEVICE_STATE_SUSPENDED)) {
		ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
		if (ret < 0) {
			LOG_WRN("%s: resume failed: %d", dev->name, ret);
		}
	}

	host_pd_child_unlock(dev, key);

	return 0;
}

/* Drop the claims children held on the domain itself */
static void host_pd_release(const struct device *dev)
{
	struct pm_device *pm = dev->pm;

	(void)k_sem_take(&pm->lock, K_FOREVER);
	pm->base.usage = 0U;
	pm->base.state = PM_DEVICE_STATE_SUSPENDED;
	k_sem_give(&pm->lock);
}

int host_power_domain_set(const struct device *dev, bool powered)
{
	struct host_pd_data *data;

	if (!device_is_ready(dev)) {
		return -ENODEV;
	}

	data = dev->data;
	(void)k_mutex_lock(&data->lock, K_FOREVER);

	if (data->synced && ((bool)atomic_get(&data->powered) == powered)) {
		k_mutex_unlock(&data->lock);
		return 0;
	}

	if (powered) {
		/* Power the children up before letting users claim the domain */
		(void)device_supported_foreach(dev, host_pd_turn_on_child,
					       (void *)dev);
		atomic_set(&data->powered, 1);
	} else {
		/* Refuse new claims first, then shut the children down */
		atomic_set(&data->powered, 0);
		(void)device_supported_foreach(dev, host_pd_turn_off_child,
					       (void *)dev);
		host_pd_release(dev);
	}

	data->synced = true;
	LOG_INF("%s: host power %s", dev->name, powered ? "on" : "off");

	k_mutex_unlock(&data->lock);

	return 0;
}

bool host_power_domain_is_powered(const struct device *dev)
{
	const struct host_pd_data *data = dev->data;

	return atomic_get(&data->powered) != 0;
}

static int host_pd_pm_action(const struct device *dev,
			     enum pm_device_action action)
{
	struct host_pd_data *data = dev->data;

	switch (action) {
	case PM_DEVICE_ACTION_RESUME:
		/* A child's runtime get() lands here: refuse while host is off */
		if (!atomic_get(&data->powered)) {
			return -EAGAIN;
		}
		break;
	case PM_DEVICE_ACTION_SUSPEND:
		break;
	case PM_DEVICE_ACTION_TURN_ON:
	case PM_DEVICE_ACTION_TURN_OFF:
		/* Not itself on a domain */
		return -ENOTSUP;
	default:
		return -ENOTSUP;
	}

	return 0;
}

static int host_pd_init(const struct device *dev)
{
	struct host_pd_data *data = dev->data;

	k_mutex_init(&data->lock);
	atomic_set(&data->powered, 0);
	data->synced = false;

	pm_device_init_suspended(dev);

	return pm_device_runtime_enable(dev);
}

#define HOST_PD_DEFINE(inst)							\
	static struct host_pd_data host_pd_data_##inst;			\
										\
	PM_DEVICE_DT_INST_DEFINE(inst, host_pd_pm_action);			\
										\
	DEVICE_DT_INST_DEFINE(inst,						\
			      host_pd_init,					\
			      PM_DEVICE_DT_INST_GET(inst),			\
			      &host_pd_data_##inst,				\
			      NULL,						\
			      POST_KERNEL,					\
			      CONFIG_HOST_POWER_DOMAIN_INIT_PRIORITY,		\
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(HOST_PD_DEFINE)
