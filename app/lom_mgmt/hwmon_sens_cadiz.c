/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>

#include "hwmon_sens.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

static struct hwmon_sram_entry_desc cadiz_sens_descs[LOM_SENSOR_MAX] = {
	/* hwmon_sdata[16]: 0x100 */
	{  8/*0x100*/, hwmon_in,    1 }, /* 12V             */
	{  9/*0x120*/, hwmon_in,    2 }, /* 5V              */
	{ 10/*0x140*/, hwmon_in,    3 }, /* 3.3V            */
	{ 11/*0x160*/, hwmon_in,    4 }, /* 0.85V           */

	{ 13/*0x1a0*/, hwmon_in,    5 }, /* 1.8V Processor  */

	{ 17/*0x220*/, hwmon_in,    6 }, /* 1.8V            */
	{ 18/*0x240*/, hwmon_curr,  7 }, /* EXP_B_POWER_MON */
	{ 19/*0x260*/, hwmon_in,    8 }, /* VCCIN_AUX       */
	{ 20/*0x280*/, hwmon_in,    9 }, /* 1.065V VDD2     */
	{ 21/*0x2a0*/, hwmon_curr, 10 }, /* VOUT_PWR_MON    */
	{ 22/*0x2c0*/, hwmon_in,   11 }, /* VDDQ 0.5V       */

	{ 24/*0x300*/, hwmon_temp, 12 }, /* PECI_CPU_Temp   */

	/* hwmon_fdata[4]:  0x320 */
	{ 25/*0x320*/, hwmon_fan,  13 }, /* Fan1_Speed    */
	{ 26/*0x340*/, hwmon_fan,  14 }, /* Fan2_Speed    */
	{ 27/*0x360*/, hwmon_fan,  15 }, /* Fan3_Speed    */
	{ 28/*0x380*/, hwmon_fan,  16 }, /* Fan4_Speed    */
	{ 29/*0x3A0*/, hwmon_fan,  17 }, /* Fan5_Speed    */
	{ 30/*0x3C0*/, hwmon_fan,  18 }, /* Fan6_Speed    */

	{ 0xFFFF, 0, 0 },
};


int lom_mgmt_sens_desc_init(struct hwmon_sram_entry_desc ** descs)
{
	*descs = cadiz_sens_descs;

	DUMP_SENS_DESCS(*descs);

	LOG_INF("Cadiz sensor table init OK");

	return 0;
}
