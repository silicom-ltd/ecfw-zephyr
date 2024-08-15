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
	uint32_t mon_in;
	uint16_t mon_sts;
	uint32_t mon_max;
	uint32_t mon_min;
	uint32_t mon_hyst;
};

struct hwmon_fdata {
	uint32_t fan_rpm;
	uint16_t fan_sts;
	uint16_t fan_pwm;
	uint16_t fanin_cfg;
	uint16_t fan_err;
};

struct hwmon_sram {
	uint8_t rsvd[0x100];
	struct hwmon_sdata mon[16];
	struct hwmon_fdata fan[2];
	
	enum cfg[16];		/* 0x238-0x248 */	/* presuming not compiling with -fshort-enums */
	char sname[16][32];	/* 0x248-0x448 */	/* friendly names are only 32 bytes max */
	char fname[2][32];
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
