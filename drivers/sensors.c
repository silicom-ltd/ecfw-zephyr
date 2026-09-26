/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <soc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/fan.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include <zephyr/drivers/adc/current_sense_amplifier.h>
#include "hwmon.h"
#include "board_config.h"
#include "peci_hub.h"
#include "pwrplane.h"
#include "system.h"

struct hwmon_sram *hwmon_data;

//LOG_MODULE_REGISTER(thrmsens, CONFIG_THERMAL_SENSOR_LOG_LEVEL);
LOG_MODULE_REGISTER(thrmsens, 4);

/*
 * Not every declared sensor is populated on every board revision (e.g. an
 * optional rail monitor left unpopulated, or a part swapped between revs).
 * Probe each one once at init so an absent sensor is skipped by the periodic
 * update paths below instead of logging a fetch error every cycle.
 */
static bool probe_sensor(const struct device *dev, enum sensor_channel chan,
			  const char *name)
{
	struct sensor_value val;
	int err;

	if (!device_is_ready(dev)) {
		LOG_WRN("Sensor %s: device not ready, marking absent", name);
		return false;
	}

	err = sensor_sample_fetch_chan(dev, chan);
	if (!err)
		err = sensor_channel_get(dev, chan, &val);

	if (err)
		LOG_WRN("Sensor %s: not responding (err %d), marking absent", name, err);

	return err == 0;
}

/*
 * The board-sensors node (out_of_tree_boards/.../mec172x_adl_n_cadiz.dts)
 * curates, per hwmon class, which already-declared sensor channels are
 * reported to the host and in what order. Reading it here instead of
 * enumerating every enabled node of a given compatible (voltage-divider,
 * current-sense-amplifier, ntc-thermistor) keeps board membership and
 * reporting order explicit rather than implied by devicetree node order.
 * See out_of_tree_boards/.../dts/bindings/sensor/silicom,board-sensors.yaml
 */
#define BOARD_SENSORS_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(silicom_board_sensors)

#define THERMAL_SENSOR_DECLARE(node_id, prop, idx) \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#if DT_NODE_HAS_PROP(BOARD_SENSORS_NODE, board_die_temp_sensors)
#define BOARD_DIE_TEMP_SENSORS_INIT \
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_die_temp_sensors, THERMAL_SENSOR_DECLARE)
#else
#define BOARD_DIE_TEMP_SENSORS_INIT
#endif

#if DT_NODE_HAS_PROP(BOARD_SENSORS_NODE, board_amb_temp_sensors)
#define BOARD_AMB_TEMP_SENSORS_INIT \
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_amb_temp_sensors, THERMAL_SENSOR_DECLARE)
#else
#define BOARD_AMB_TEMP_SENSORS_INIT
#endif

static const struct device *ntc_thermal_sensors[] = {
	BOARD_DIE_TEMP_SENSORS_INIT
	BOARD_AMB_TEMP_SENSORS_INIT
};

static bool thermal_sensor_valid[ARRAY_SIZE(ntc_thermal_sensors)];

static const struct device *max31785i2c = DEVICE_DT_GET(DT_NODELABEL(max31785_i2c3));
static bool max31785_cpu_valid;

int thermal_sensors_init(void)
{
	int i, num_sensors = ARRAY_SIZE(ntc_thermal_sensors);
	int present = 0;

	for (i = 0; i < num_sensors; i++) {
		thermal_sensor_valid[i] = probe_sensor(ntc_thermal_sensors[i],
			SENSOR_CHAN_AMBIENT_TEMP, ntc_thermal_sensors[i]->name);
		if (thermal_sensor_valid[i])
			present++;
	}
	LOG_INF("NTC thermal sensors: %d/%d present", present, num_sensors);

	max31785_cpu_valid = probe_sensor(max31785i2c, SENSOR_CHAN_AMBIENT_TEMP,
		"MAX31785 CPU");

	return 0;
}

void thermal_sensors_update(void)
{
	int i, num_sensors = ARRAY_SIZE(ntc_thermal_sensors);
	struct sensor_value temp;
	unsigned int temp_val;
	int multiplier, old_multiplier;
	struct adc_dt_spec *adc_dt;
	volatile struct hwmon_sdata *sdata;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}

	if (max31785_cpu_valid) {
		err = sensor_sample_fetch_chan(max31785i2c, SENSOR_CHAN_AMBIENT_TEMP);
		if (!err)
			err = sensor_channel_get(max31785i2c, SENSOR_CHAN_AMBIENT_TEMP, &temp);
		if (!err)
			LOG_INF("MAX31785 CPU read %d.%03dC", temp.val1, temp.val2);
		else
			LOG_WRN("MAX31785 CPU read failed: %d", err);
	}

	for (i = 0; i < num_sensors; i++) {
		if (!thermal_sensor_valid[i])
			continue;

		adc_dt = (struct adc_dt_spec *)ntc_thermal_sensors[i]->config;

		sdata = &hwmon_data->mon[adc_dt->channel_cfg.channel_id];

		err = sensor_sample_fetch_chan(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP);
		if (err) {
			LOG_WRN("ADC Sensor reading failed %d, %s\n", i, ntc_thermal_sensors[i]->name);
			continue;
		}
		sensor_channel_get(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP, &temp);
		LOG_DBG("Sensor %d thermistor read: %d.%03dC", adc_dt->channel_cfg.channel_id, temp.val1, temp.val2);

		old_multiplier = sdata->multiplier;
		multiplier = 0;
		temp_val = (temp.val1 * 1000) + (temp.val2 / 1000);
		while (temp_val & 0xFFFF0000) {
			temp_val >>= 1;
			if (!multiplier)
				multiplier = 1;
			else
				multiplier <<= 1;
		}

		sdata->mon_in = temp_val;
		if (multiplier > old_multiplier) {
			sdata->multiplier = multiplier;
			sdata->mon_max = temp_val;
		} else if (sdata->mon_in > sdata->mon_max) {
		LOG_DBG("\tUpdating memory @ 0x%08x with %d, mult: %d\n",(unsigned int)&sdata->mon_in, sdata->mon_in, multiplier);
			sdata->mon_max = sdata->mon_in;
		}
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (multiplier < old_multiplier) {
			sdata->multiplier = multiplier;
			sdata->mon_min = temp_val;
		}

		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
	}
}

#define VOLTAGE_SENSOR_DECLARE(node_id, prop, idx) \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define VOLTAGE_SENSOR_DT_DECLARE(node_id, prop, idx) \
	VOLTAGE_DIVIDER_DT_SPEC_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *voltage_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_volt_sensors, VOLTAGE_SENSOR_DECLARE)
};

static struct voltage_divider_dt_spec data[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_volt_sensors, VOLTAGE_SENSOR_DT_DECLARE)
};

static bool voltage_sensor_valid[ARRAY_SIZE(voltage_sensors)];

int voltage_monitor_init(void)
{
	int i, num_sensors = ARRAY_SIZE(voltage_sensors);
	int present = 0;

	for (i = 0; i < num_sensors; i++) {
		voltage_sensor_valid[i] = probe_sensor(voltage_sensors[i],
			SENSOR_CHAN_VOLTAGE, voltage_sensors[i]->name);
		if (voltage_sensor_valid[i])
			present++;
	}
	LOG_INF("Voltage sensors: %d/%d present", present, num_sensors);

	return 0;
}


void voltage_monitor_update(void)
{
	int i, num_sensors = ARRAY_SIZE(voltage_sensors);
	struct sensor_value volts;
	struct voltage_divider_dt_spec *voltage;
	volatile struct hwmon_sdata *sdata;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}
	else {
		LOG_INF("ESPI EC1_SRAM: 0x%08x\n",(uint32_t)hwmon_data);
	}

	for (i = 0; i < num_sensors; i++) {
		if (!voltage_sensor_valid[i])
			continue;

		voltage = &data[i];

		sdata = &hwmon_data->mon[voltage->port.channel_id];

		err = sensor_sample_fetch_chan(voltage_sensors[i], SENSOR_CHAN_VOLTAGE);
		if (err) {
			LOG_WRN("ADC voltage reading failed %d, %s\n", i, voltage_sensors[i]->name);
			continue;
		}
		sensor_channel_get(voltage_sensors[i], SENSOR_CHAN_VOLTAGE, &volts);
		LOG_DBG("ADC %d voltage read: %d.%03d V", voltage->port.channel_id, volts.val1, volts.val2);

		//sdata->mon_in = (volts.val1 << 16) | (volts.val2 / 16);
		sdata->mon_in = (volts.val1 * 1000) + (volts.val2 / 1000);
		LOG_DBG("\tUpdating memory @ 0x%08x with %d\n",(unsigned int)&sdata->mon_in, sdata->mon_in);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
		sdata->multiplier = 0;
	}
}

#define CURRENT_SENSOR_DECLARE(node_id, prop, idx) \
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

#define CURRENT_SENSOR_DT_DECLARE(node_id, prop, idx) \
	CURRENT_SENSE_AMPLIFIER_DT_SPEC_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *current_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_curr_sensors, CURRENT_SENSOR_DECLARE)
};

static struct current_sense_amplifier_dt_spec current_data[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, board_curr_sensors, CURRENT_SENSOR_DT_DECLARE)
};

static bool current_sensor_valid[ARRAY_SIZE(current_sensors)];

int current_sense_init(void)
{
	int i, num_sensors = ARRAY_SIZE(current_sensors);
	int present = 0;

	for (i = 0; i < num_sensors; i++) {
		current_sensor_valid[i] = probe_sensor(current_sensors[i],
			SENSOR_CHAN_CURRENT, current_sensors[i]->name);
		if (current_sensor_valid[i])
			present++;
	}
	LOG_INF("Current sensors: %d/%d present", present, num_sensors);

	return 0;
}

void current_sense_update(void)
{
	int i, num_sensors = ARRAY_SIZE(current_sensors);
	struct sensor_value amps;
	struct current_sense_amplifier_dt_spec *current;
	volatile struct hwmon_sdata *sdata;
	unsigned int temp_val;
	int multiplier, old_multiplier;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}

	for (i = 0; i < num_sensors; i++) {
		if (!current_sensor_valid[i])
			continue;

		current = &current_data[i];

		sdata = &hwmon_data->mon[current->port.channel_id];

		err = sensor_sample_fetch_chan(current_sensors[i], SENSOR_CHAN_CURRENT);
		if (err) {
			LOG_WRN("ADC current reading failed %d, %s\n", i, current_sensors[i]->name);
			continue;
		}
		sensor_channel_get(current_sensors[i], SENSOR_CHAN_CURRENT, &amps);
		LOG_DBG("ADC %d current read: %d.%d mA", current->port.channel_id, amps.val1, amps.val2);

		old_multiplier = sdata->multiplier;
		multiplier = 0;
		temp_val = amps.val1;
		while (temp_val & 0xFFFF0000) {
			temp_val >>= 1;
			if (!multiplier)
				multiplier = 1;
			else
				multiplier <<= 1;
		}
		sdata->mon_in = temp_val;
		sdata->multiplier = multiplier;

		LOG_DBG("\tUpdating memory @ 0x%08x with %d\n", (unsigned int)&sdata->mon_in, sdata->mon_in);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_min + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
	}
}

/*
 * CPU die temperature via PECI, into hwmon_data->peci -- a single dedicated
 * struct for this one reading, distinct from the mon[] array the other
 * board-*-sensors above populate.
 *
 * Two mutually exclusive mechanisms exist in this codebase -- enforced
 * mutually exclusive by "depends on !DT_HAS_X86_PECI_TEMP_ENABLED" on
 * CONFIG_PECI_OVER_ESPI_ENABLE (app/thermal_management/Kconfig), so at most
 * one of these two branches is ever compiled in:
 *  - devicetree enumeration of the normal Zephyr sensor-model driver
 *    (../zephyr/drivers/sensor/x86_peci_temp, compatible "x86-peci-temp"),
 *    which is what this board (cadiz) actually uses: board-peci-temp-sensor
 *    is resolved with DEVICE_DT_GET and read like the other board-*-sensors
 *    above via sensor_sample_fetch/channel_get;
 *  - CONFIG_PECI_OVER_ESPI_ENABLE, where drivers/peci_hub.c's own
 *    PECI-over-eSPI OOB implementation is used instead -- it doesn't go
 *    through the Zephyr device model at all and resolves its own PECI bus
 *    device internally via PECI_0_INST, so board-peci-temp-sensor's target
 *    isn't actually consumed there; it only gates whether this board
 *    reports PECI CPU temp at all.
 */
#if DT_NODE_HAS_PROP(BOARD_SENSORS_NODE, board_peci_temp_sensor)

#if DT_HAS_COMPAT_STATUS_OKAY(x86_peci_temp)

static const struct device *peci_cpu_temp_dev =
	DEVICE_DT_GET(DT_PHANDLE(BOARD_SENSORS_NODE, board_peci_temp_sensor));

/*
 * Unlike the other board-*-sensors above, this isn't probe_sensor()'d once
 * at init: PECI only responds once the CPU is powered (S0), which isn't
 * guaranteed yet at sensors_init() time, and a probe failure there would
 * permanently mark it absent. Instead peci_temp_update() checks system
 * state on every call and only reads it in S0.
 */
int peci_temp_init(void)
{
	if (!device_is_ready(peci_cpu_temp_dev)) {
		LOG_WRN("board-peci-temp-sensor (%s): device not ready",
			peci_cpu_temp_dev->name);
		return -ENODEV;
	}

	return 0;
}

void peci_temp_update(void)
{
	struct sensor_value temp;
	volatile struct hwmon_peci *pdata;
	unsigned int temp_val;
	int multiplier;
	int err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}

	if (pwrseq_system_state() != SYSTEM_S0_STATE) {
		LOG_DBG("PECI CPU temp skipped, system state %d != SYSTEM_S0_STATE",
			pwrseq_system_state());
		return; // PECI only responds to the CPU while it's powered
	}

	err = sensor_sample_fetch_chan(peci_cpu_temp_dev, SENSOR_CHAN_DIE_TEMP);
	if (!err)
		err = sensor_channel_get(peci_cpu_temp_dev, SENSOR_CHAN_DIE_TEMP, &temp);
	if (err) {
		LOG_WRN("PECI CPU temp read failed: %d", err);
		return;
	}
	LOG_DBG("PECI CPU temp: %d.%03d C", temp.val1, temp.val2);

	multiplier = 0;
	temp_val = (temp.val1 * 1000) + (temp.val2 / 1000);
	while (temp_val & 0xFFFF0000) {
		temp_val >>= 1;
		if (!multiplier)
			multiplier = 1;
		else
			multiplier <<= 1;
	}

	pdata = &hwmon_data->peci;
	pdata->peci_in = temp_val;
	pdata->multiplier = multiplier;
}

#elif defined(CONFIG_PECI_OVER_ESPI_ENABLE)

int peci_temp_init(void)
{
	int err = peci_init();

	if (err)
		LOG_WRN("PECI init failed: %d, CPU temp will not be reported", err);

	return err;
}

void peci_temp_update(void)
{
	int temp, err;

	if (hwmon_data == NULL) {
		return; // espi emi not configured yet
	}

	err = peci_get_temp(CPU, &temp);
	if (err) {
		LOG_WRN("PECI CPU temp read failed: %d", err);
		return;
	}

	LOG_DBG("PECI CPU temp: %d C", temp);
}

#endif /* DT_HAS_COMPAT_STATUS_OKAY(x86_peci_temp) vs CONFIG_PECI_OVER_ESPI_ENABLE */

#endif /* DT_NODE_HAS_PROP(BOARD_SENSORS_NODE, board_peci_temp_sensor) */

static const struct device *fan_dev[] = {
        DEVICE_DT_GET(DT_ALIAS(fan0)),
        DEVICE_DT_GET(DT_ALIAS(fan1)),
        DEVICE_DT_GET(DT_ALIAS(fan2)),
        DEVICE_DT_GET(DT_ALIAS(fan3)),
        DEVICE_DT_GET(DT_ALIAS(fan4)),
        DEVICE_DT_GET(DT_ALIAS(fan5)),
};

void fan_update(void)
{
	int ret;
	int i;
	struct sensor_value val;
	struct hwmon_fdata *fdata;

	for (i = 0; i < ARRAY_SIZE(fan_dev); i++) {

		ret = fan_get_speed(fan_dev[i], &val);
		LOG_DBG("fan index %d, name: %s, speed: %d",i, fan_dev[i]->name, val.val1);

		if (ret != 0)
			return;

		if (hwmon_data == NULL)
			return;

		fdata = &hwmon_data->fan[i];
		fdata->fan_rpm = val.val1;
	}

}

static const struct device *espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

struct espi_callback pltrst_cb;

static void init_hwmon_data(const struct device *dev, struct espi_callback *cb,
			    struct espi_event event)
{
	int ret;
	uint32_t hwmon_d = 0;

	if (event.evt_type == ESPI_BUS_EVENT_VWIRE_RECEIVED) {
		if ((event.evt_details == ESPI_VWIRE_SIGNAL_PLTRST)) {
			if ((event.evt_data == 1)) {
				ret = espi_read_lpc_request(espi_dev, EMI1_GET_SHARED_MEMORY, &hwmon_d);
				if (ret != 0)
					LOG_INF("Error %d returned from read_lpc_request", ret);
				hwmon_data = (struct hwmon_sram *)hwmon_d;
			}
		}
	}
}

int sensors_init()
{
	espi_init_callback(&pltrst_cb, init_hwmon_data,
			ESPI_BUS_EVENT_VWIRE_RECEIVED);
	espi_add_callback(espi_dev, &pltrst_cb);

	thermal_sensors_init();
	voltage_monitor_init();
	current_sense_init();
#if DT_NODE_HAS_PROP(BOARD_SENSORS_NODE, board_peci_temp_sensor)
	peci_temp_init();
#endif

	return 0;
}

void sensors_update()
{
	voltage_monitor_update();
	thermal_sensors_update();
	current_sense_update();
	fan_update();
#if DT_NODE_HAS_PROP(BOARD_SENSORS_NODE, board_peci_temp_sensor)
	peci_temp_update();
#endif
}
