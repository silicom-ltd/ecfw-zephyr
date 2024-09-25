/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_H__
#define __HWMON_H__

#define THERMISTOR_TYPE 16
#define VOLTAGE_TYPE 96
#define CURRENT_TYPE 32
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

struct hwmon_sdata {
	uint16_t mon_in;	/* 0x0 */
	uint16_t mon_sts;	/* 0x2 */
	uint16_t mon_max;	/* 0x4 */
	uint16_t mon_crit;	/* 0x6 */
	uint16_t mon_hyst;	/* 0x8 */
	uint16_t mon_min;	/* 0xa */
	uint16_t rsvd[2];	/* 0xc */
};

struct hwmon_peci {
	uint8_t peci_in;	/* 0x0, degrees C resolution */
	uint8_t peci_tjmax;	/* 0x1 */
	uint16_t peci_raw;	/* 0x2 2s complement 1/64 degree */
};

struct hwmon_fdata {
	uint16_t fan_rpm;
	uint16_t fan_min;
	uint16_t fan_alarm;
	uint16_t fanin_cfg;
};

struct hwmon_pdata {
	uint8_t pwm_in;
	uint8_t pwm_en;
};

struct hwmon_sram {
	uint8_t rsvd[0x100];
	struct hwmon_sdata mon[16];
	struct hwmon_peci;	
	uint32_t rsvd[3];
	struct hwmon_fdata fan[2];
	struct hwmon_pdata pwm[2];	/* only for pwm-controlled fan */
};

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
int sensors_update(void);

#endif	/* __HWMON_H__ */
