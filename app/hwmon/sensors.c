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
#include <zephyr/drivers/adc/voltage_divider.h>
#include <zephyr/drivers/adc/current_sense_amplifier.h>
#include "hwmon.h"
#include "board_config.h"

#include "sensors.h"

struct hwmon_sram *hwmon_data;

LOG_MODULE_REGISTER(thrmsens, CONFIG_THERMAL_SENSOR_LOG_LEVEL);

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
/*
 * hwmon only polls the ADC thermistors explicitly listed on the
 * "silicom,board-sensors" node's adc-temp-sensors property, instead of
 * sweeping every murata,ncp15xh103-compatible node in the tree.
 */
#define ADC_TEMP_SENSOR_DECLARE(node_id, prop, idx)			\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *ntc_thermal_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, adc_temp_sensors, ADC_TEMP_SENSOR_DECLARE)
};

BUILD_ASSERT(ARRAY_SIZE(ntc_thermal_sensors) == ADC_TEMP_SENSOR_NUM,
	"Invalid size of ntc_thermal_sensors");
#else
#define DT_DRV_COMPAT murata_ncp15xh103

#define THERMAL_SENSOR(inst) \
	DEVICE_DT_GET(DT_NODELABEL(therm##inst)),

static const struct device *ntc_thermal_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(THERMAL_SENSOR)
};

#undef DT_DRV_COMPAT
#endif

static void maestro_ddr5_vtt_sodimm(struct sensor_value *volts);

int thermal_sensors_init()
{
	int i;
	int num_sensors = ARRAY_SIZE(ntc_thermal_sensors);

	for (i = 0; i < num_sensors; i++) {
#if 0
		LOG_INF("Sensor %d, name: %s\n", i, ntc_thermal_sensors[i]->name);
		adc = &ntc_thermal_sensors[i].config;

		hwmon->cfg[adc->channel_id] = adc->channel_id + THERMISTOR_TYPE
#endif
	}

	return 0;
}

/* used for debug */
//static uint64_t upd_count_1 = 0;
//static uint64_t upd_count_2 = 0;
//static uint64_t upd_count_3 = 0;

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

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
	board_thermal_sensors_update();
#endif

	for (i = 0; i < num_sensors; i++) {
		adc_dt = (struct adc_dt_spec *)ntc_thermal_sensors[i]->config;

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
		/* Sized/indexed from adc-temp-sensors; see hwmon.h. */
		sdata = &hwmon_data->adc_mon_thermal[i];
#else
		sdata = &hwmon_data->mon[adc_dt->channel_cfg.channel_id];
#endif

		err = sensor_sample_fetch_chan(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP);
		if (err) {
			LOG_WRN("ADC (%02u) thermistor reading failed %s\n", adc_dt->channel_cfg.channel_id, ntc_thermal_sensors[i]->name);
			continue;
		}
		sensor_channel_get(ntc_thermal_sensors[i], SENSOR_CHAN_AMBIENT_TEMP, &temp);
		LOG_INF("ADC (%02u) thermistor read: %d.%03dC", adc_dt->channel_cfg.channel_id, temp.val1, temp.val2);

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
//		LOG_INF("\tUpdating memory @ 0x%08x with %d, mult: %d\n",(unsigned int)&sdata->mon_in, sdata->mon_in, multiplier);
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

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
/*
 * hwmon only polls the ADC voltage dividers explicitly listed on the
 * "silicom,board-sensors" node's adc-volt-sensors property, instead of
 * sweeping every voltage-divider-compatible node in the tree. Both arrays
 * are built from that same list, so they stay paired by construction.
 */
#define ADC_VOLT_SENSOR_DEV_DECLARE(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),
#define ADC_VOLT_SENSOR_SPEC_DECLARE(node_id, prop, idx)		\
	VOLTAGE_DIVIDER_DT_SPEC_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *voltage_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, adc_volt_sensors, ADC_VOLT_SENSOR_DEV_DECLARE)
};

static struct voltage_divider_dt_spec data[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, adc_volt_sensors, ADC_VOLT_SENSOR_SPEC_DECLARE)
};

BUILD_ASSERT(ARRAY_SIZE(voltage_sensors) == ADC_VOLT_SENSOR_NUM,
	"Invalid size of voltage_sensors");
#else
#define DT_DRV_COMPAT voltage_divider

#define VOLTAGE_MONITOR_DT(inst)			\
	VOLTAGE_DIVIDER_DT_SPEC_GET(DT_NODELABEL(voltage##inst)),

#define GET_DEVICE_BY_NODE(node_id)	DEVICE_DT_GET(node_id),
static const struct device *voltage_sensors[] = {
	DT_FOREACH_STATUS_OKAY(voltage_divider, GET_DEVICE_BY_NODE)
};

static struct voltage_divider_dt_spec data[] = {
	DT_INST_FOREACH_STATUS_OKAY(VOLTAGE_MONITOR_DT)
};

#undef DT_DRV_COMPAT
#endif

int voltage_monitor_init(void)
{
	int i;
	int num_sensors = ARRAY_SIZE(voltage_sensors);

	for (i = 0; i < num_sensors; i++) {

#if 0
		LOG_INF("Sensor %d, name: %s\n", i, voltage_sensors[i]->name);
		hwmon->cfg[voltage->port.channel_id] = voltage->port.channel_id + VOLTAGE_TYPE;
#endif
	}

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

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
	board_voltage_sensors_update();
#endif

	for (i = 0; i < num_sensors; i++) {
		voltage = &data[i];

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
		/* Sized/indexed from adc-volt-sensors; see hwmon.h. */
		sdata = &hwmon_data->adc_mon_voltage[i];
#else
		sdata = &hwmon_data->mon[voltage->port.channel_id];
#endif

		err = sensor_sample_fetch_chan(voltage_sensors[i], SENSOR_CHAN_VOLTAGE);
		if (err) {
			LOG_WRN("ADC (%02u) voltage reading failed, %s\n", voltage->port.channel_id, voltage_sensors[i]->name);
			continue;
		}
		sensor_channel_get(voltage_sensors[i], SENSOR_CHAN_VOLTAGE, &volts);

        /**
            The call to maestro_ddr5_vtt_sodimm() function will correct
            VTT_SODIMM voltage reading for DDR5 board due to addition of
            the voltage divider.
        **/
        if (voltage->port.channel_id == MAESTRO_VTT_SODIMM_ADC_CHNL)
        {
            if (get_bom_id() == MAESTRO_DDR5_BOM_ID)
            {
                maestro_ddr5_vtt_sodimm (&volts);
            }
        }

		LOG_INF("ADC (%02u) voltage read: %d.%06d V", voltage->port.channel_id, volts.val1, volts.val2);

		//sdata->mon_in = (volts.val1 << 16) | (volts.val2 / 16);
		sdata->mon_in = (volts.val1 * 1000) + (volts.val2 / 1000);
//		LOG_INF("\tUpdating memory @ 0x%08x with %d\n",(unsigned int)&sdata->mon_in, sdata->mon_in);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_in + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
		sdata->multiplier = 0;
	}
}

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
/*
 * hwmon only polls the ADC current-sense amplifiers explicitly listed on the
 * "silicom,board-sensors" node's adc-curr-sensors property, instead of
 * sweeping every current-sense-amplifier-compatible node in the tree. Both
 * arrays are built from that same list, so they stay paired by construction.
 */
#define ADC_CURR_SENSOR_DEV_DECLARE(node_id, prop, idx)		\
	DEVICE_DT_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),
#define ADC_CURR_SENSOR_SPEC_DECLARE(node_id, prop, idx)		\
	CURRENT_SENSE_AMPLIFIER_DT_SPEC_GET(DT_PHANDLE_BY_IDX(node_id, prop, idx)),

static const struct device *current_sensors[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, adc_curr_sensors, ADC_CURR_SENSOR_DEV_DECLARE)
};

static struct current_sense_amplifier_dt_spec current_data[] = {
	DT_FOREACH_PROP_ELEM(BOARD_SENSORS_NODE, adc_curr_sensors, ADC_CURR_SENSOR_SPEC_DECLARE)
};

BUILD_ASSERT(ARRAY_SIZE(current_sensors) == ADC_CURR_SENSOR_NUM,
	"Invalid size of current_sensors");
#else
#define DT_DRV_COMPAT current_sense_amplifier
#define CURRENT_SENSOR(inst)	\
       DEVICE_DT_GET(DT_NODELABEL(current##inst))

#define CURRENT_SENSE_DT(inst)			\
	CURRENT_SENSE_AMPLIFIER_DT_SPEC_GET(DT_NODELABEL(current##inst)),

static const struct device *current_sensors[] = {
	DT_INST_FOREACH_STATUS_OKAY(CURRENT_SENSOR)
};

static struct current_sense_amplifier_dt_spec current_data[] = {
	DT_INST_FOREACH_STATUS_OKAY(CURRENT_SENSE_DT)
};

#undef DT_DRV_COMPAT
#endif

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

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
	board_current_sensors_update();
	board_power_sensors_update();
#endif

	for (i = 0; i < num_sensors; i++) {
		current = &current_data[i];

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
		/* Sized/indexed from adc-curr-sensors; see hwmon.h. */
		sdata = &hwmon_data->adc_mon_current[i];
#else
		sdata = &hwmon_data->mon[current->port.channel_id];
#endif

		err = sensor_sample_fetch_chan(current_sensors[i], SENSOR_CHAN_CURRENT);
		if (err) {
			LOG_WRN("ADC (%02u) current reading failed, %s\n", current->port.channel_id, current_sensors[i]->name);
			continue;
		}
		sensor_channel_get(current_sensors[i], SENSOR_CHAN_CURRENT, &amps);
		LOG_INF("ADC (%02u) current read: %d.%03d mA", current->port.channel_id, amps.val1, amps.val2);

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

		LOG_INF("\tUpdating memory @ 0x%08x with %d\n", (unsigned int)&sdata->mon_in, sdata->mon_in);
		if (sdata->mon_in > sdata->mon_max)
			sdata->mon_max = sdata->mon_in;
		if ((sdata->mon_min == 0) || (sdata->mon_in < sdata->mon_min))
			sdata->mon_min = sdata->mon_in;
		if (sdata->mon_min + sdata->mon_hyst)
			; /* place saver for hysteresis action ? */
	}
}

#undef DT_DRV_COMPAT

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

#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
				board_sensors_hwmon_setting();
#endif
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

	return 0;
}

void sensors_update()
{
	voltage_monitor_update();
	thermal_sensors_update();
	current_sense_update();
}

/**
    This function converts the volt to milivolt,
    then apply the voltage divider formula to get
    the input voltage and converts back to volt.
**/
static void maestro_ddr5_vtt_sodimm (struct sensor_value *volts)
{
    // R1 resistor -> 1K
    // R2 resistor -> 1K
    //uint16_t output_ohms = 10000;
    //uint16_t full_ohms = 10000+10000;

    uint32_t v_mv = (volts->val1 * 1000) + (volts->val2 / 1000);
    //v_mv = (v_mv * full_ohms) / output_ohms;
    v_mv = v_mv * 2;

    volts->val1 = v_mv / 1000;
    volts->val2 = (v_mv * 1000) % 1000000;
}
