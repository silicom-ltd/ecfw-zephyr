/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_H__
#define __HWMON_H__


#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
#include <hwmon_cp.h>
#endif

#define THERMISTOR_TYPE 16
#define VOLTAGE_TYPE 96
#define CURRENT_TYPE 32
#if 0
enum sensor_type {
	NULL,	/* unimplemented */
	thermistor0 = THERMISTOR_TYPE,
	thermistor1,
	thermistor2,
	thermistor3,
	thermistor4,
	thermistor5,
	thermistor6,
	thermistor7,
	thermistor8,
	thermistor9,
	thermistor10,
	thermistor11,
	thermistor12,
	thermistor13,
	thermistor14,
	thermistor15,
	current0 = CURRENT_TYPE,
	current1,
	current2,
	current3,
	current4,
	current5,
	current6,
	current7,
	current8,
	current9,
	current10,
	current11,
	current12,
	current13,
	current14,
	current15,
	vin0 = VOLTAGE_TYPE,
	vin1,
	vin2,
	vin3,
	vin4,
	vin5,
	vin6,
	vin7,
	vin8,
	vin9,
	vin10,
	vin11,
	vin12,
	vin13,
	vin14,
	vin15,
};
#endif

enum sensor_types {
	hwmon_chip,
	hwmon_temp,
	hwmon_in,
	hwmon_curr,
	hwmon_power,
	hwmon_energy,
	hwmon_humidity,
	hwmon_fan,
	hwmon_pwm,
	hwmon_intrusion,
	hwmon_max,
};

struct hwmon_sdata {
	uint16_t mon_in;	/* 0x0 */
	uint16_t mon_max;	/* 0x2 */
	uint16_t mon_crit;	/* 0x4 */
	uint16_t mon_hyst;	/* 0x6 */
	uint16_t mon_min;	/* 0x8 */
	uint16_t mon_alarm;	/* 0xa */
	uint16_t mon_target;	/* 0xc */
	uint16_t multiplier;	/* 0xe */
	uint16_t mon_sts;	/* 0x10*/
	uint16_t mon_cfg;	/* 0x12 */
	uint16_t type;		/* 0x14 */
} __attribute__ ((packed, aligned(32)));

struct hwmon_peci {
	uint16_t peci_in;	/* 0x0, degrees C resolution */
	uint16_t :16;
	uint16_t peci_tjmax;	/* 0x2 */
	uint16_t peci_raw;	/* 0x4 2s complement 1/64 degree */
	uint16_t rsvd[3];
	uint16_t multiplier;
} __attribute__ ((packed, aligned(32)));

struct hwmon_fdata {
	uint16_t fan_rpm;
	uint16_t fan_max;
	uint16_t rsvd0;
	uint16_t rsvd1;
	uint16_t fan_min;
	uint16_t fan_alarm;
	uint16_t fan_target;
	uint16_t rsvd2;
	uint16_t fan_sts;
	uint16_t fan_cfg;
} __attribute__ ((packed, aligned(32)));

struct hwmon_pdata {
	uint16_t pwm_in;
} __attribute__ ((packed, aligned(32)));


struct hwmon_sram {
	uint8_t rsvd[0x100];
#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
	/*
	 * Sized and indexed (0..N-1, declaration order) from the
	 * "silicom,board-sensors" node's adc-*-sensors properties, rather
	 * than the SoC's fixed 16-channel ADC and raw channel-number
	 * addressing mon[] below uses for boards without that node.
	 */
	struct hwmon_sdata adc_mon_thermal[ADC_TEMP_SENSOR_NUM];
	struct hwmon_sdata adc_mon_voltage[ADC_VOLT_SENSOR_NUM];
	struct hwmon_sdata adc_mon_current[ADC_CURR_SENSOR_NUM];
#else
	struct hwmon_sdata mon[16];
#endif
	struct hwmon_peci peci;
	struct hwmon_fdata fan[4];
	struct hwmon_pdata pwm[4];	/* only for pwm-controlled fan */
#ifdef CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED
	struct hwmon_fdata board_fan[BOARD_FAN_NUM];
	struct hwmon_sdata board_mon_thermal[BOARD_THERMAL_SENSOR_NUM];
	struct hwmon_sdata board_mon_voltage[BOARD_VOLTAGE_SENSOR_NUM];
	struct hwmon_sdata board_mon_current[BOARD_CURRENT_SENSOR_NUM];
	struct hwmon_sdata board_mon_power[BOARD_POWER_SENSOR_NUM];
#endif
} __attribute__ ((packed, aligned(32)));

#define HWMON_SRAM_ENTRY_IDX(entry_ptr, hwmon_data)			\
	(((uintptr_t)entry_ptr - (uintptr_t)hwmon_data)/32)

#define SET_HWMON_SRAM_ENTRY_TYPE(hwmon_data, entry_ptr, type)		\
	do {								\
		int idx = HWMON_SRAM_ENTRY_IDX(entry_ptr, hwmon_data);	\
		__ASSERT(idx < sizeof(hwmon_data->rsvd),		\
			"Out of range of hwmon sram entry idx %d", idx); \
		hwmon_data->rsvd[idx] = type;				\
	} while (0)

//struct hwmon_sram *hwmon_data;

int voltage_monitor_init(void);
void voltage_monitor_update(void);
void current_sense_update(void);

/**
 * @brief Initialize sensor module.
 *
 * @return 0 if success, otherwise error code.
 */
int sensors_init(void);


/**
 * @brief  Read all the ADC sensors.
 *
 * This function call reads all ADC sensors enabled in init, and
 * updates hwmon struct for respective ADC channel reads.
 */
void sensors_update(void);

#endif	/* __HWMON_H__ */
