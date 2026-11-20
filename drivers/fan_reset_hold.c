/*
 * Copyright (c) 2026 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Fan controller reset hold (silicom,fan-reset-hold).
 *
 * While a fan controller such as the MAX31785 is in reset, its PWM outputs
 * float high and the fans run at 100%. The board devicetree holds it there
 * from EC boot (reset GPIO hogged low) and defers its device inits
 * (zephyr,deferred-init), so nothing configures it.
 *
 * Each silicom,fan-reset-hold node names the fan controller and the sensors
 * it depends on (ready-sensors). Once every ready sensor reports a real
 * reading, this releases the reset and runs the controller's deferred inits,
 * once per EC boot. There is no timeout: a sensor that never becomes valid
 * keeps the fans at 100%. If an init fails, the controller is put back in
 * reset, since a deferred init cannot be retried and a half-configured
 * controller may leave the fans disabled.
 *
 * On Cadiz the ready sensor is the LM75 target the MAX31785 reads, which is
 * only ready once its own backing sensor (PECI CPU temperature) answers.
 */

#define DT_DRV_COMPAT silicom_fan_reset_hold

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include "fan_reset_hold.h"
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_i2c_target_lm75)
#include <zephyr/drivers/i2c/target/lm75.h>
#endif

LOG_MODULE_REGISTER(fan_hold, CONFIG_FAN_RESET_HOLD_LOG_LEVEL);

#define FAN_HOLD_BOOT_MS	100	/* MAX31785 answered ~35 ms after release */

struct fan_hold_ready {
	const struct device *dev;
	bool lm75_target;
};

struct fan_hold_config {
	const char *name;
	struct gpio_dt_spec reset;
	const struct device *const *devs;
	size_t ndevs;
	const struct fan_hold_ready *ready;
	size_t nready;
};

struct fan_hold_data {
	const struct fan_hold_config *config;
	struct k_work_delayable work;
	atomic_t released;
};

static K_THREAD_STACK_DEFINE(fan_hold_stack, 1024);
static struct k_work_q fan_hold_wq;

static bool fan_hold_sensor_ready(const struct fan_hold_ready *r)
{
	if (!device_is_ready(r->dev)) {
		return false;
	}
#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_i2c_target_lm75)
	if (r->lm75_target) {
		return lm75_target_sensor_valid(r->dev);
	}
#endif
	return sensor_sample_fetch(r->dev) == 0;
}

static void fan_hold_release(const struct fan_hold_config *cfg)
{
	int err;

	err = gpio_pin_set_dt(&cfg->reset, 1);
	if (err) {
		LOG_ERR("%s: reset release failed: %d, fans stay at 100%%", cfg->name, err);
		return;
	}
	LOG_INF("%s: temperature sources valid, reset released", cfg->name);

	k_msleep(FAN_HOLD_BOOT_MS);

	for (size_t i = 0; i < cfg->ndevs; i++) {
		const struct device *dev = cfg->devs[i];

		if (dev == NULL || device_is_ready(dev)) {
			continue;	/* no device, or not deferred */
		}

		err = device_init(dev);
		if (err) {
			(void)gpio_pin_set_dt(&cfg->reset, 0);
			LOG_ERR("%s: %s init failed: %d, back in reset, fans at 100%%",
				cfg->name, dev->name, err);
			return;
		}
	}

	LOG_INF("%s: fan control active", cfg->name);
}

static void fan_hold_poll(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct fan_hold_data *data = CONTAINER_OF(dwork, struct fan_hold_data, work);
	const struct fan_hold_config *cfg = data->config;

	for (size_t i = 0; i < cfg->nready; i++) {
		if (!fan_hold_sensor_ready(&cfg->ready[i])) {
			k_work_reschedule_for_queue(&fan_hold_wq, dwork,
						    K_MSEC(CONFIG_FAN_RESET_HOLD_POLL_MS));
			return;
		}
	}

	atomic_set(&data->released, 1);
	fan_hold_release(cfg);
}

/* Fan controller, its children (fans/sensors containers), then their
 * children (fan0-5, i2c0-3): the normal MFD -> fan -> sensor init order.
 * Nodes without a device give NULL entries.
 */
#define FAN_HOLD_DEV(node)	DEVICE_DT_GET_OR_NULL(node),
#define FAN_HOLD_SUBTREE(node)	DT_FOREACH_CHILD(node, FAN_HOLD_DEV)
#define FAN_HOLD_CTRL(inst)	DT_INST_PHANDLE(inst, fan_controller)

#define FAN_HOLD_READY(node, prop, idx)						\
	{									\
		.dev = DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node, prop, idx)),	\
		.lm75_target = DT_NODE_HAS_COMPAT(DT_PHANDLE_BY_IDX(node, prop, idx), \
						  zephyr_i2c_target_lm75),	\
	},

#define FAN_HOLD_DEFINE(inst)							\
	static const struct device *const fan_hold_devs_##inst[] = {		\
		DEVICE_DT_GET(FAN_HOLD_CTRL(inst)),				\
		DT_FOREACH_CHILD_STATUS_OKAY(FAN_HOLD_CTRL(inst), FAN_HOLD_DEV)	\
		DT_FOREACH_CHILD_STATUS_OKAY(FAN_HOLD_CTRL(inst), FAN_HOLD_SUBTREE) \
	};									\
	static const struct fan_hold_ready fan_hold_ready_##inst[] = {		\
		DT_INST_FOREACH_PROP_ELEM(inst, ready_sensors, FAN_HOLD_READY)	\
	};									\
	static const struct fan_hold_config fan_hold_config_##inst = {		\
		.name = DT_NODE_FULL_NAME(FAN_HOLD_CTRL(inst)),			\
		.reset = GPIO_DT_SPEC_GET(FAN_HOLD_CTRL(inst), reset_gpios),	\
		.devs = fan_hold_devs_##inst,					\
		.ndevs = ARRAY_SIZE(fan_hold_devs_##inst),			\
		.ready = fan_hold_ready_##inst,					\
		.nready = ARRAY_SIZE(fan_hold_ready_##inst),			\
	};									\
	static struct fan_hold_data fan_hold_data_##inst = {			\
		.config = &fan_hold_config_##inst,				\
	};

DT_INST_FOREACH_STATUS_OKAY(FAN_HOLD_DEFINE)

#define FAN_HOLD_DATA_PTR(inst)	&fan_hold_data_##inst,

static struct fan_hold_data *const fan_hold_all[] = {
	DT_INST_FOREACH_STATUS_OKAY(FAN_HOLD_DATA_PTR)
};

bool fan_reset_hold_active(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(fan_hold_all); i++) {
		if (!atomic_get(&fan_hold_all[i]->released)) {
			return true;
		}
	}
	return false;
}

static int fan_reset_hold_init(void)
{
	k_work_queue_start(&fan_hold_wq, fan_hold_stack, K_THREAD_STACK_SIZEOF(fan_hold_stack),
			   K_PRIO_PREEMPT(10), NULL);
	k_thread_name_set(&fan_hold_wq.thread, "fan_hold");

	for (size_t i = 0; i < ARRAY_SIZE(fan_hold_all); i++) {
		k_work_init_delayable(&fan_hold_all[i]->work, fan_hold_poll);
		k_work_schedule_for_queue(&fan_hold_wq, &fan_hold_all[i]->work, K_NO_WAIT);
	}

	return 0;
}

SYS_INIT(fan_reset_hold_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);
