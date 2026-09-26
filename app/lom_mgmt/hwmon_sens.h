/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _HWMON_SENS_H_
#define _HWMON_SENS_H_

#include <stdint.h>

#include "hwmon.h"
#include "lom_mgmt_proc_inc.h"

#define SENS_DTYPE_INTEG 0
#define SENS_DTYPE_FLOAT 1

#define HWMON_SDATA_MUL_OFF 7

struct hwmon_sram_entry_desc {
	uint16_t          entry_idx;
	enum sensor_types sens_type;
	uint8_t           sens_id;
};

#if CONFIG_LOM_MGMT_PROC_LOG_LEVEL >= 4
static inline const char *sens_type_name(enum sensor_types typ)
{
	switch (typ) {
	case hwmon_chip:      return "CHP";
	case hwmon_temp:      return "TMP";
	case hwmon_in:        return "VOL";
	case hwmon_curr:      return "CUR";
	case hwmon_power:     return "PWR";
	case hwmon_energy:    return "ENG";
	case hwmon_humidity:  return "HUM";
	case hwmon_fan:       return "FAN";
	case hwmon_pwm:       return "PWM";
	case hwmon_intrusion: return "INT";
	default:              break;
	}

	return "INV";
}

#define DUMP_SENS_DESCS(p_descs)					    \
	do {								    \
		LOG_DBG_SENS("SENS_DESC[IDX]: TYP SID IDX:OFF@hwmon");      \
		for (int i = 0; (p_descs)[i].entry_idx != 0xFFFF; i++) {    \
			LOG_DBG_SENS("SENS_DESC[%3d]: %s %3d %3d:0x%x", i,  \
				sens_type_name((p_descs)[i].sens_type),	    \
				(p_descs)[i].sens_id,			    \
				(p_descs)[i].entry_idx,			    \
				((p_descs)[i].entry_idx - 8) * 32 + 0x100); \
		}							    \
	}								    \
	while (0)
#else
#define DUMP_SENS_DESCS(p_descs) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE
struct sensor_record {
	uint8_t  info; /* b[7:6]:type, b[5:0]: multiplier */
	uint8_t  sens_id;
	uint16_t value;
} __attribute__((__packed__));

#define SENS_INFO_MUL_BIT 0
#define SENS_INFO_TYP_BIT 6 /* data type(2bits): 0: integer, 1: float */

#define SENS_INFO_MUL_MASK 0x3F
#define SENS_INFO_TYP_MASK 0x03

#define SRD_SENS_ID(srd) ((srd)->sens_id)

#define SET_SRD_SENS_ID(srd, val)		\
	do {					\
		(srd)->sens_id = (val);		\
	} while (0)
#else
struct sensor_record {
	uint8_t info; /* b[7]:type, b[6]:multiplier, b[5:0]: sensor id */
	uint16_t value;
} __attribute__((__packed__));

#define SENS_INFO_MUL_BIT 6
#define SENS_INFO_TYP_BIT 7 /* data type(1bits): 0: integer, 1: float */

#define SENS_INFO_MUL_MASK 0x01
#define SENS_INFO_TYP_MASK 0x01
#define SENS_ID_MASK       0x3F

#define SRD_SENS_ID(srd) ((srd)->info & SENS_ID_MASK)

#define SET_SRD_SENS_ID(srd, val)		\
	do {					\
		(srd)->info |= (val);		\
	} while (0)
#endif

#define SRD_SENS_MUL(srd) (((srd)->info >> SENS_INFO_MUL_BIT) & SENS_INFO_MUL_MASK)
#define SRD_SENS_TYP(srd) (((srd)->info >> SENS_INFO_TYP_BIT) & SENS_INFO_TYP_MASK)

#define SENSOR_RECORD_SIZE (sizeof(struct sensor_record))

#define LOM_SENSOR_MAX 256

int lom_mgmt_sens_desc_init(struct hwmon_sram_entry_desc** descs);

#endif
