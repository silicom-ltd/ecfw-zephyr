/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>

#include "hwmon_cp.h"
#include "hwmon_sens.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#if defined(CONFIG_BOARD_MEC172X_ADL_N_CP) && !defined(CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE)
#error "Error: CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE must be enabled"
#endif

/*
  The new added sensors always be stored in contiguous memory locations and at the end,
  so the sensor_id could be calculated by sequence
*/
static struct hwmon_sram_entry_desc maestro_sens_descs[LOM_SENSOR_MAX] = {
	/* hwmon_sdata[16]: 0x100 */
	{  8/*0x100*/, hwmon_in,    6 }, /* 12V */
	{  9/*0x120*/, hwmon_in,    7 }, /* 5V */
	{ 10/*0x140*/, hwmon_in,    8 }, /* 3.3V Always On */
	{ 11/*0x160*/, hwmon_in,    9 }, /* 1.8V Always On */
	{ 12/*0x180*/, hwmon_temp,  1 }, /* Ambient */
	{ 13/*0x1a0*/, hwmon_temp,  2 }, /* Core VR */
	{ 14/*0x1c0*/, hwmon_temp,  3 }, /* DDR */
	{ 17/*0x220*/, hwmon_in,   10 }, /* 1.8V */
	{ 18/*0x240*/, hwmon_temp,  4 }, /* CPU */
	{ 19/*0x260*/, hwmon_in,   11 }, /* VCCIN_AUX */
	{ 20/*0x280*/, hwmon_in,   12 }, /* 1.2V_VDD2 */
	{ 22/*0x2c0*/, hwmon_in,   14 }, /* VTT_SODIMM */
	{ 23/*0x2e0*/, hwmon_curr, 17 }, /* Board Power */

	/* hwmon_peci:      0x300 */
	{ 24/*0x300*/, hwmon_temp,  5 }, /* CPU PECI */

	/*
	 * !! new sens_id should start from 18 !!
	 */

	/* hwmon.emc230x_fan@33, 8 entries*/
	{ 33/*0x420*/, hwmon_fan,  18 }, /* Fan1_RPM    */
	{ 34/*0x440*/, hwmon_fan,  19 }, /* Fan2_RPM    */
	{ 35/*0x460*/, hwmon_fan,  20 }, /* Fan3_RPM    */
	{ 36/*0x480*/, hwmon_fan,  21 }, /* Fan4_RPM    */
	{ 37/*0x4a0*/, hwmon_fan,  22 }, /* Fan5_RPM    */
	{ 38/*0x4c0*/, hwmon_fan,  23 }, /* Fan6_RPM    */
	{ 39/*0x4e0*/, hwmon_fan,  24 }, /* Fan7_RPM    */
	{ 40/*0x500*/, hwmon_fan,  25 }, /* Fan8_RPM    */

	{ 0xFFFF, 0, 0 },
};

#define DYN_SENSOR_HWMON_IDX_BASE 43

int lom_mgmt_sens_desc_init(struct hwmon_sram_entry_desc ** descs)
{
	int start_idx;
	int used_max_sens_id = 0;
	int entry_idx = DYN_SENSOR_HWMON_IDX_BASE;
	int i;

	for (i = 0; i < LOM_SENSOR_MAX; i++) {
		if (maestro_sens_descs[i].entry_idx == 0xFFFF) {
			break;
		}
		if (maestro_sens_descs[i].sens_id > used_max_sens_id) {
			used_max_sens_id = maestro_sens_descs[i].sens_id;
		}
	}
	start_idx = i;

	for (i = 0; i < SW_THERMAL_SENSOR_NUM; i++, entry_idx++) {
		maestro_sens_descs[start_idx + i].entry_idx = entry_idx;
		maestro_sens_descs[start_idx + i].sens_type = hwmon_temp;
		maestro_sens_descs[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<TEMP> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			maestro_sens_descs[start_idx + i].entry_idx,
			maestro_sens_descs[start_idx + i].sens_type,
			maestro_sens_descs[start_idx + i].sens_id);
	}
	start_idx += SW_THERMAL_SENSOR_NUM;

	for (i = 0; i < SW_VOLTAGE_SENSOR_NUM; i++, entry_idx++) {
		maestro_sens_descs[start_idx + i].entry_idx = entry_idx;
		maestro_sens_descs[start_idx + i].sens_type = hwmon_in;
		maestro_sens_descs[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<VOLT> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			maestro_sens_descs[start_idx + i].entry_idx,
			maestro_sens_descs[start_idx + i].sens_type,
			maestro_sens_descs[start_idx + i].sens_id);
	}
	start_idx += SW_VOLTAGE_SENSOR_NUM;

	for (i = 0; i < SW_CURRENT_SENSOR_NUM; i++, entry_idx++) {
		maestro_sens_descs[start_idx + i].entry_idx = entry_idx;
		maestro_sens_descs[start_idx + i].sens_type = hwmon_curr;
		maestro_sens_descs[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<CURR> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			maestro_sens_descs[start_idx + i].entry_idx,
			maestro_sens_descs[start_idx + i].sens_type,
			maestro_sens_descs[start_idx + i].sens_id);
	}
	start_idx += SW_CURRENT_SENSOR_NUM;

	for (i = 0; i < SW_POWER_SENSOR_NUM; i++, entry_idx++) {
		maestro_sens_descs[start_idx + i].entry_idx = entry_idx;
		maestro_sens_descs[start_idx + i].sens_type = hwmon_power;
		maestro_sens_descs[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<POWR> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			maestro_sens_descs[start_idx + i].entry_idx,
			maestro_sens_descs[start_idx + i].sens_type,
			maestro_sens_descs[start_idx + i].sens_id);
	}
	start_idx += SW_POWER_SENSOR_NUM;

	maestro_sens_descs[start_idx].entry_idx = 0xFFFF;

	LOG_DBG_SENS("<LAST> SENS[%02d]: { -1, %d, %3d }", start_idx,
		maestro_sens_descs[start_idx].sens_type,
		maestro_sens_descs[start_idx].sens_id);


	*descs = maestro_sens_descs;

	DUMP_SENS_DESCS(*descs);

	LOG_INF("Maestro sensor table init OK");

	return 0;
}
