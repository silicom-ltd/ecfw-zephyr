/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>

#include "hwmon_sens.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

static struct hwmon_sram_entry_desc ibiza_sens_descs[LOM_SENSOR_MAX] = {
       /* hwmon_sdata[16]: 0x100 */
       {  8/*0x100*/, hwmon_in,    6 }, /* P12V0A        */
       {  9/*0x120*/, hwmon_in,    7 }, /* P5V0A         */
       { 10/*0x140*/, hwmon_in,    8 }, /* P3V3_ALW_ON   */
       { 11/*0x160*/, hwmon_in,    9 }, /* P1V8_ALW_ON   */
       { 12/*0x180*/, hwmon_temp,  1 }, /* AmbientTemp   */
       { 13/*0x1a0*/, hwmon_temp,  2 }, /* VR_Temp       */
       { 14/*0x1c0*/, hwmon_temp,  3 }, /* DDR_Temp      */
       { 17/*0x220*/, hwmon_in,   10 }, /* P1V8A         */
       { 18/*0x240*/, hwmon_temp,  4 }, /* CPU_Temp      */
       { 19/*0x260*/, hwmon_in,   11 }, /* VCCIN_AUX     */
       { 20/*0x280*/, hwmon_in,   12 }, /* 1.2V_VDD2     */
       { 21/*0x2a0*/, hwmon_in,   13 }, /* P0V95S        */
       { 22/*0x2c0*/, hwmon_in,   14 }, /* VTT_SODIMM    */

       { 23/*0x2e0*/, hwmon_curr, 17 }, /* PWR_MON       */

       /* hwmon_peci:      0x300 */
       { 24/*0x300*/, hwmon_temp,  5 }, /* CPU_PECI_Temp */

       /* hwmon_fdata[4]:  0x320 */
       { 25/*0x320*/, hwmon_fan,  15 }, /* Fan1_Speed    */
       { 26/*0x340*/, hwmon_fan,  16 }, /* Fan2_Speed    */

       { 0xFFFF, 0, 0 },
};


int lom_mgmt_sens_desc_init(struct hwmon_sram_entry_desc ** descs)
{
	*descs = ibiza_sens_descs;

	DUMP_SENS_DESCS(*descs);

	LOG_INF("Ibiza sensor table init OK");

	return 0;
}
