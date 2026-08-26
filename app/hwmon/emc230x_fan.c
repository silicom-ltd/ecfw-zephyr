/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2023-2024 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/fan.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include "hwmon.h"
#include "gpio_ec.h"
#include "board_config.h"
#include "emc230x_fan.h"

LOG_MODULE_REGISTER(fan, CONFIG_FAN_LOG_LEVEL);

extern struct hwmon_sram *hwmon_data;

#define MAX_DUTY_CYCLE		100u
#define EMC230X_FAN_DEFAULT_DUTY_CYCLE 50u

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
/*
 * hwmon only polls the fan channels explicitly listed on the
 * "silicom,board-sensors" node's fan-devices property, instead of assuming
 * aliases fan0..fan7 all exist.
 */
#define FAN_DEV_DECLARE(node_id, prop, idx)				\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *emc230x_fan_dev[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, fan_devices, FAN_DEV_DECLARE)
};
#else
static const struct device *emc230x_fan_dev[] = {
	DEVICE_DT_GET(DT_ALIAS(fan0)),
	DEVICE_DT_GET(DT_ALIAS(fan1)),
	DEVICE_DT_GET(DT_ALIAS(fan2)),
	DEVICE_DT_GET(DT_ALIAS(fan3)),
	DEVICE_DT_GET(DT_ALIAS(fan4)),
	DEVICE_DT_GET(DT_ALIAS(fan5)),
	DEVICE_DT_GET(DT_ALIAS(fan6)),
	DEVICE_DT_GET(DT_ALIAS(fan7)),
};
#endif

int fan_init(void)
{
	LOG_WRN("Emc20x_fan_dev size %d", ARRAY_SIZE(emc230x_fan_dev));	
	return ARRAY_SIZE(emc230x_fan_dev);
}

int fan_power_set(bool power_state)
{
	/* fan disable circuitry is active low. */
	return gpio_write_pin(FAN_PWR_DISABLE_N, power_state);
}

int fan_set_duty_cycle(enum fan_type fan_idx, uint8_t rpm)
{
	int ret;
	struct hwmon_fdata *fdata;

	if (fan_idx > ARRAY_SIZE(emc230x_fan_dev)) {
		return -EINVAL;
	}

	if (rpm > MAX_DUTY_CYCLE) {
		rpm = MAX_DUTY_CYCLE;
	}

	const struct device *fan = emc230x_fan_dev[fan_idx];

	LOG_WRN("Fan %d setting duty cycle %d", fan_idx, rpm);
	ret = fan_set_cycles(fan, (uint32_t)rpm);

	if (ret) {
		LOG_WRN("Fan setting error: %d", ret);
		return ret;
	}

	if (hwmon_data == NULL)
		return 0;

	fdata = &hwmon_data->emc230x_fan[fan_idx];
	fdata->fan_target = rpm;

	return 0;
}

int fan_read_rpm(enum fan_type fan_idx, uint16_t *rpm)
{
	struct hwmon_fdata *fdata;
       
	if (fan_idx > ARRAY_SIZE(emc230x_fan_dev))
		return -ENODEV;

	fdata = &hwmon_data->emc230x_fan[fan_idx];

	*rpm = fdata->fan_rpm;

	return 0;
}

void fans_turn_off(struct k_timer *timer_id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(emc230x_fan_dev); i++)
		fan_set_duty_cycle(i, 0);
}

K_TIMER_DEFINE(fan_off_timer, fans_turn_off, NULL);

void fans_spin_down(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(emc230x_fan_dev); i++) {
		fan_set_duty_cycle(i, 15);
	}
	k_timer_start(&fan_off_timer, K_SECONDS(30), K_NO_WAIT);
}

void fans_set_default(void)
{
	int i;

	k_timer_stop(&fan_off_timer);

	for (i = 0; i < ARRAY_SIZE(emc230x_fan_dev); i++)
		fan_set_duty_cycle(i, EMC230X_FAN_DEFAULT_DUTY_CYCLE);
}


int fan_count(void)
{
	return ARRAY_SIZE(emc230x_fan_dev);
}

uint16_t fan_hwmon_idx(int fan_idx)
{
	__ASSERT(fan_idx >= 0 && fan_idx < ARRAY_SIZE(emc230x_fan_dev),
		"index %d out of range", fan_idx);
	return HWMON_SRAM_ENTRY_IDX(&hwmon_data->emc230x_fan[fan_idx], hwmon_data);
}

int fan_update(void)
{
	int ret;
	int i;
	struct sensor_value val;
	struct hwmon_fdata *fdata;

	for (i = 0; i < ARRAY_SIZE(emc230x_fan_dev); i++) {

		ret = fan_get_speed(emc230x_fan_dev[i], &val);
		LOG_DBG("fan index %d, name: %s, speed: %d",i, emc230x_fan_dev[i]->name, val.val1);

		if (ret != 0)
			return ret;

		if (hwmon_data == NULL)
			return 0;

		fdata = &hwmon_data->emc230x_fan[i];
		fdata->fan_rpm = val.val1;
#if 0
		if ((fdata->fan_target != 0) && (fdata->fan_target <= 100))
			fan_set_duty_cycle(i, (uint8_t)fdata->fan_target);
#endif
	}

	return 0;
}
