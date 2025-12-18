/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "board_config.h"
#include "smchost.h"
#include "hwmon.h"
#include "hwmon_cp.h"

#include "pwrplane.h"
#include "pwrbtnmgmt.h"
#include "espi_hub.h"
#include "task_handler.h"
#include "rstbutton.h"

#include "host_event.h"
#include "lom_mgmt_i2c.h"


LOG_MODULE_REGISTER(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#if (CONFIG_SW_SENSOR_LOG_LEVEL >= LOG_LEVEL_DBG)
#define LOM_MGMT_DBG LOG_INF
#else
#define LOM_MGMT_DBG
#endif
//#define SENSOR_DRY_RUN

#define CPU_TEMP_CS_ACCESS_PERIOD_SEC 8U

static const struct device *const espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

extern struct hwmon_sram *hwmon_data;

#define SENS_DTYPE_INTEG 0
#define SENS_DTYPE_FLOAT 1

struct hwmon_sram_entry_desc {
	uint16_t          entry_idx;
	enum sensor_types sens_type;
	uint8_t           sens_id;
};

#define SENS_INFO_MUL_BIT 0
#define SENS_INFO_TYP_BIT 6 /* data type(2bits): 0: integer, 1: float */

/*
   The new added sensors always be stored in contiguous memory locations and at the end,
   so the sensor_id could be calculated by sequence
*/
#if 0
static const struct hwmon_sram_entry_desc hwmon_entries_mho150[] = {
	/* hwmon_sdata[16]: 0x100 */
	{  8/*0x100*/, hwmon_in,    6 }, /* P12V0A        */ /* 12V */
	{  9/*0x120*/, hwmon_in,    7 }, /* P5V0A         */ /* 5V */
	{ 10/*0x140*/, hwmon_in,    8 }, /* P3V3_ALW_ON   */ /* 3.3V Always On */
	{ 11/*0x160*/, hwmon_in,    9 }, /* P1V8_ALW_ON   */ /* 1.8V Always On */
	{ 12/*0x180*/, hwmon_temp,  1 }, /* AmbientTemp   */ /* Ambient */
	{ 13/*0x1a0*/, hwmon_temp,  2 }, /* VR_Temp       */ /* Core VR */
	{ 14/*0x1c0*/, hwmon_temp,  3 }, /* DDR_Temp      */ /* DDR */
	{ 17/*0x220*/, hwmon_in,   10 }, /* P1V8A         */ /* 1.8V */
	{ 18/*0x240*/, hwmon_temp,  4 }, /* CPU_Temp      */ /* CPU */
	{ 19/*0x260*/, hwmon_in,   11 }, /* VCCIN_AUX     */ /* VCCINAUX */
	{ 20/*0x280*/, hwmon_in,   12 }, /* 1.2V_VDD2     */ /* 1.2V_VDD2 */
	//{ 21/*0x2a0*/, hwmon_in,   13 }, /* P0V95S        -*/
	{ 22/*0x2c0*/, hwmon_in,   14 }, /* VTT_SODIMM    */ /* VTT_SODIMM */
	{ 23/*0x2e0*/, hwmon_curr, 17 }, /* PWR_MON       */ /* Board Power */

	/* hwmon_peci:      0x300 */
	{ 24/*0x300*/, hwmon_temp,  5 }, /* CPU_PECI_Temp */ /* CPU PECI */

	/* hwmon_fdata[4]:  0x320 */
	//{ 25/*0x320*/, hwmon_fan, 15 }, /* Fan1_Speed    */
	//{ 26/*0x340*/, hwmon_fan, 16 }, /* Fan2_Speed    */

	/*
	 * !! From here the Max sens_id is 17 !!
	 */

	/* hwmon.emc230x_fan, 8 entries*/
	{ 33/*0x420*/, hwmon_fan,  18 }, /* Fan1_RPM    */
	{ 34/*0x440*/, hwmon_fan,  00 }, /* Fan2_RPM    */
	{ 35/*0x460*/, hwmon_fan,  00 }, /* Fan3_RPM    */
	{ 36/*0x480*/, hwmon_fan,  00 }, /* Fan4_RPM    */
	{ 37/*0x4a0*/, hwmon_fan,  00 }, /* Fan5_RPM    */
	{ 38/*0x4c0*/, hwmon_fan,  00 }, /* Fan6_RPM    */
	{ 39/*0x4e0*/, hwmon_fan,  00 }, /* Fan7_RPM    */
	{ 40/*0x500*/, hwmon_fan,  00 }, /* Fan8_RPM    */

	/* hwmon.sw_mon_thermal, 8 entries*/
	{ 43/*0x560*/, hwmon_temp, 00 }, /* mfd3_temp     : 3.3V Left(id 26) */
	{ 44/*0x580*/, hwmon_temp, 00 }, /* mfd4_temp     : 3.3V Right */
	{ 45/*0x5a0*/, hwmon_temp, 00 }, /* mfd5_temp     : VDDH 1.1V */
	{ 46/*0x5c0*/, hwmon_temp, 00 }, /* mfd6_temp_amb : PSU1 Left Ambient */
	{ 47/*0x5e0*/, hwmon_temp, 00 }, /* mfd6_temp_hot : PSU1 Left Hot */
	{ 48/*0x600*/, hwmon_temp, 00 }, /* mfd7_temp     : Main 3x Rails */
	{ 49/*0x620*/, hwmon_temp, 00 }, /* mfd8_temp_amb : PSU2 Right Ambient */
	{ 50/*0x640*/, hwmon_temp, 00 }, /* mfd8_temp_hot : PSU2 Right Hot*/
	{ 51/*0x660*/, hwmon_temp, 00 }, /* switch_temp0  : Switch T0 */
	{ 52/*0x680*/, hwmon_temp, 00 }, /* switch_temp1  : Switch T1 */

	/* hwmon.sw_mon_voltage, 9 entries */
	{ 53/*0x6a0*/, hwmon_in,   00 },  /* mfd3_vin     : 3.3V Left Vin (id 36) */
	{ 54/*0x6c0*/, hwmon_in,   00 },  /* mfd3_vout    : 3.3V Left Vout */
	{ 55/*0x6e0*/, hwmon_in,   00 },  /* mfd4_vin     : 3.3V Right Vin */
	{ 56/*0x700*/, hwmon_in,   00 },  /* mfd4_vout    : 3.3V Right Vout */
	{ 57/*0x720*/, hwmon_in,   00 },  /* mfd5_vin     : VDDH 1.1V Vin */
	{ 58/*0x740*/, hwmon_in,   00 },  /* mfd5_vout    : VDDH 1.1V Vout */
	{ 59/*0x760*/, hwmon_in,   00 },  /* mfd7_r0_vout : Core VDD */
	{ 60/*0x780*/, hwmon_in,   00 },  /* mfd7_r1_vout : GOP VDD .8V */
	{ 61/*0x7a0*/, hwmon_in,   00 },  /* mfd7_r2_vout : VDDA .9V */

	/* hwmon.sw_mon_current, 5 entries */
	{ 62/*0x7c0*/, hwmon_curr, 00 },  /* mfd3_iout    : 3.3V Left Iout (id 45) */
	{ 63/*0x7e0*/, hwmon_curr, 00 },  /* mfd4_iout    : 3.3V Right Iout */
	{ 64/*0x800*/, hwmon_curr, 00 },  /* mfd5_iout    : VDDH 1.1V Iout */
	{ 65/*0x820*/, hwmon_curr, 00 },  /* mfd6_iout    : PSU1 Left Iout */
	{ 66/*0x840*/, hwmon_curr, 00 },  /* mfd7_r0_iout : Core Iout */
	{ 67/*0x860*/, hwmon_curr, 00 },  /* mfd7_r1_iout : GOP Iout */
	{ 68/*0x880*/, hwmon_curr, 00 },  /* mfd7_r2_iout : VDDA Iout */
	{ 69/*0x8a0*/, hwmon_curr, 00 },  /* mfd8_iout    : PSU2 Right Iout */

	/* hwmon.sw_mon_power, 2 entries */
	{ 70/*0x8c0*/, hwmon_power, 00 },  /* mfd6_pin    : PSU1 Left Pwr(id 54) */
	{ 71/*0x8e0*/, hwmon_power, 00 },  /* mfd8_pin    : PSU2 Right Pwr */

	{ 0xFFFF, 0, 0 }
};
#endif

#define LOM_SENSOR_MAX 256
#define DYN_SENSOR_ENTRY_IDX_BASE 43
#define DYN_SENSOR_ID_BASE 18

static struct hwmon_sram_entry_desc hwmon_entries[LOM_SENSOR_MAX] = {
	/* hwmon_sdata[16]: 0x100 */
	{  8/*0x100*/, hwmon_in,    6 }, /* P12V0A        */ /* 12V */
	{  9/*0x120*/, hwmon_in,    7 }, /* P5V0A         */ /* 5V */
	{ 10/*0x140*/, hwmon_in,    8 }, /* P3V3_ALW_ON   */ /* 3.3V Always On */
	{ 11/*0x160*/, hwmon_in,    9 }, /* P1V8_ALW_ON   */ /* 1.8V Always On */
	{ 12/*0x180*/, hwmon_temp,  1 }, /* AmbientTemp   */ /* Ambient */
	{ 13/*0x1a0*/, hwmon_temp,  2 }, /* VR_Temp       */ /* Core VR */
	{ 14/*0x1c0*/, hwmon_temp,  3 }, /* DDR_Temp      */ /* DDR */
	{ 17/*0x220*/, hwmon_in,   10 }, /* P1V8A         */ /* 1.8V */
	{ 18/*0x240*/, hwmon_temp,  4 }, /* CPU_Temp      */ /* CPU */
	{ 19/*0x260*/, hwmon_in,   11 }, /* VCCIN_AUX     */ /* VCCIN_AUX */
	{ 20/*0x280*/, hwmon_in,   12 }, /* 1.2V_VDD2     */ /* 1.2V_VDD2 */
	//{ 21/*0x2a0*/, hwmon_in,   SENS_DTYPE_FLOAT, 13 }, /* P0V95S        -*/
	{ 22/*0x2c0*/, hwmon_in,   14 }, /* VTT_SODIMM    */ /* VTT_SODIMM */
	{ 23/*0x2e0*/, hwmon_curr, 17 }, /* PWR_MON       */ /* Board Power */

	/* hwmon_peci:      0x300 */
	{ 24/*0x300*/, hwmon_temp,  5 }, /* CPU_PECI_Temp */ /* CPU PECI */

	/* hwmon_fdata[4]:  0x320 */
	//{ 25/*0x320*/, hwmon_fan, 15 }, /* Fan1_Speed    */
	//{ 26/*0x340*/, hwmon_fan, 16 }, /* Fan2_Speed    */

	/*
	 * !! new sens_id should start from 18 !!
	 */

	/* hwmon.emc230x_fan@33, 8 entries*/
	{ 33/*0x420*/, hwmon_fan,  18 }, /* Fan1_RPM    */ /*new sens_id start from 18 */
	{ 34/*0x440*/, hwmon_fan,  00 }, /* Fan2_RPM    */
	{ 35/*0x460*/, hwmon_fan,  00 }, /* Fan3_RPM    */
	{ 36/*0x480*/, hwmon_fan,  00 }, /* Fan4_RPM    */
	{ 37/*0x4a0*/, hwmon_fan,  00 }, /* Fan5_RPM    */
	{ 38/*0x4c0*/, hwmon_fan,  00 }, /* Fan6_RPM    */
	{ 39/*0x4e0*/, hwmon_fan,  00 }, /* Fan7_RPM    */
	{ 40/*0x500*/, hwmon_fan,  00 }, /* Fan8_RPM    */

	{ 0xFFFF, 0, 0 },
};

struct sensor_record {
	uint8_t  info;
	uint8_t  sens_id;
	uint16_t value;
} __attribute__((__packed__));

#define SENSOR_RECORD_SIZE (sizeof(struct sensor_record))


static const struct device *lom_mgmt_dev = DEVICE_DT_GET(DT_NODELABEL(lom_mgmt));

K_SEM_DEFINE(lom_mgmt_sem, 0, 1);

struct lom_mgmt_task {
	struct lom_mgmt_req *req;
	struct lom_mgmt_res *res;
};

static struct lom_mgmt_task lom_mgmt_task;


static void lom_mgmt_dyn_sensor_table_init(void)
{
	int start_idx;
	int entry_idx = DYN_SENSOR_ENTRY_IDX_BASE;
	int i;

	LOM_MGMT_DBG("Dynamic sensors items init");

	for (i = 0; i < LOM_SENSOR_MAX; i++) {
		if (hwmon_entries[i].entry_idx == 0xFFFF) {
			break;
		}
	}
	start_idx = i;

	for (i = 0; i < SW_THERMAL_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_temp;
		hwmon_entries[start_idx + i].sens_id   = 0;

		LOM_MGMT_DBG("<TEMP> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_THERMAL_SENSOR_NUM;

	for (i = 0; i < SW_VOLTAGE_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_in;
		hwmon_entries[start_idx + i].sens_id   = 0;

		LOM_MGMT_DBG("<VOLT> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_VOLTAGE_SENSOR_NUM;

	for (i = 0; i < SW_CURRENT_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_curr;
		hwmon_entries[start_idx + i].sens_id   = 0;

		LOM_MGMT_DBG("<CURR> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);

	}
	start_idx += SW_CURRENT_SENSOR_NUM;

	for (i = 0; i < SW_POWER_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_power;
		hwmon_entries[start_idx + i].sens_id   = 0;

		LOM_MGMT_DBG("<POWR> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_POWER_SENSOR_NUM;

	hwmon_entries[start_idx].entry_idx = 0xFFFF;

	LOM_MGMT_DBG("<LAST> SENS[%02d]: { %d, %d, %3d }", start_idx,
		hwmon_entries[start_idx].entry_idx,
		hwmon_entries[start_idx].sens_type,
		hwmon_entries[start_idx].sens_id);
}

/*
 * Callbacks for lom_mgmt_i2c
 */
static void lom_mgmt_i2c_cancel(void)
{
	return;
}

static int lom_mgmt_i2c_request(struct lom_mgmt_req *req, struct lom_mgmt_res *res)
{
	lom_mgmt_task.req = req;
	lom_mgmt_task.res = res;

	if (req == NULL || res == NULL) {
		return -1;
	}

	k_sem_give(&lom_mgmt_sem); /* wakeup handler */

	return 0;
}

static struct lom_mgmt_i2c_callbacks callbacks = {
	lom_mgmt_i2c_request,
	lom_mgmt_i2c_cancel,
};

static inline void fill_one_sensor(struct sensor_record *srd,
	const struct hwmon_sram_entry_desc * ent, uint8_t *sens_id_max)
{
	uint16_t *sram = (uint16_t *)hwmon_data;
	uint16_t offset = (ent->entry_idx * 32) / 2; /* offset in two byte unit */

	/* set sens_id */
	uint8_t sens_id = ent->sens_id;
	if (sens_id == 0) {
		sens_id = ++(*sens_id_max);
	}
	else if (sens_id > *sens_id_max) {
		*sens_id_max = sens_id;
	}
	else {
		__ASSERT(sens_id != *sens_id_max, "Duplicated sens_id %d", sens_id);
	}

	uint8_t mul = 0;
	if (ent->sens_type != hwmon_fan && ent->sens_type != hwmon_pwm) {
		mul = (uint8_t)(sram[offset + 7] & 0x1); /* mul */
	}

	if (ent->sens_type == hwmon_fan) {
		srd->info = (SENS_DTYPE_INTEG << SENS_INFO_TYP_BIT) | mul;
	}
	else {
		srd->info = (SENS_DTYPE_FLOAT << SENS_INFO_TYP_BIT) | mul;
	}

	srd->sens_id = sens_id;
	srd->value = htons(sram[offset]);
#ifdef SENSOR_DRY_RUN
	if (ent->entry_idx >= 33) {
		srd->value = htons(ent->entry_idx * 32);
	}
#endif
}

static int do_get_sensors(uint8_t *res_data, uint16_t *dat_size)
{
	struct sensor_record * srd;
	uint16_t data_off = 0;
	uint8_t sens_id_max = 0;

	for (int i = 0; hwmon_entries[i].entry_idx != 0xFFFF; i++) {
		if (data_off + SENSOR_RECORD_SIZE > RES_DLEN_MAX) {
			return -1;
		}

		srd = (struct sensor_record *)&(res_data[data_off]);

		fill_one_sensor(srd, &hwmon_entries[i], &sens_id_max);

		LOM_MGMT_DBG(">> SENS@hwmon[%03d] 0x%02x %3d 0x%04x", hwmon_entries[i].entry_idx,
			srd->info, srd->sens_id, ntohs(srd->value));

		data_off += SENSOR_RECORD_SIZE;
	}

	*dat_size = data_off;

	return 0;
}

#if 0
#define RND_CNT 16
static int delays[RND_CNT] = { 0, 10, 0, 0, 80, 0, 0, 0, 0, 150, 0, 0, 0, 110, 0, 50};
static int rnd_idx = 0;
#endif

static int do_get_events(uint8_t *res_data, uint16_t *dat_size)
{
	uint8_t * data = &res_data[4];
	int off = 0;
	int ret;

	while ((ret = host_event_get(&data[off])) > 0) {
		off += ret;
	}

	if (off)
		*dat_size = off + 4;
	else
		*dat_size = 0;

	return 0;
}

static uint8_t acpi_state[] = {
	6, /* Hard Off */
	1, /* S0 */
	3, /* S3 */
	4, /* S4 */
	5, /* S5 */
};

static int lom_mgmt_handle_request(struct lom_mgmt_task *task)
{
	if (task->req == NULL || task->res == NULL) {
		LOG_ERR(">> No Request!");
		return -1;
	}

	LOG_DBG(">> Processing request <%d>", task->req->func);

	uint8_t *req_data = task->req->data;
	uint8_t *res_data = task->res->data;
	uint8_t code = EC_RET_OK;
	uint8_t flag = 0;
	uint16_t dat_size = 0;
	int i;

	switch (task->req->func) {
	case FUNC_POWER_CTRL:
		break;
	case FUNC_GET_ACPI_POWER_STATE:
		uint8_t s = pwrseq_system_state();
		res_data[0] = acpi_state[s];
		dat_size = 1;
		break;
	case FUNC_GET_SENSORS:
		if (do_get_sensors(res_data, &dat_size) < 0) {
			LOG_ERR("request size exceeds the limit: %d", RES_DLEN_MAX);
			code = EC_RET_ERR_INV_SIZE;
			dat_size = 0;
			break;
		}

		LOG_INF("sensors data size %d", dat_size);

#if 0
		LOG_INF("+=+ 0x%02x 0x%02x 0x%02x 0x%02x",
				res_data[0], res_data[1], res_data[2], res_data[3]);
		k_sleep(K_MSEC(delays[rnd_idx++ % RND_CNT]));
#endif
		break;
	case FUNC_GET_FRU:
		break;
	case FUNC_GET_EVENTS:
		if (do_get_events(res_data, &dat_size) < 0) {
			LOG_ERR("get events failed");
			code = EC_RET_ERR_INV_SIZE;
			dat_size = 0;
			break;
		}

		if (dat_size)
			flag = RES_META_F_TIMESTAMP;

		LOG_DBG("events size %d", dat_size);

		break;
	case FUNC_TEST_L3:
		/* get test size */
		dat_size = ((uint16_t)req_data[0] << 8) | req_data[1];

		LOG_DBG("testL3 size %d", dat_size);

		if (dat_size < 4) {
			code = EC_RET_ERR_INV_SIZE;
			dat_size = 0;
			break;
		}
		else if (dat_size > RES_DLEN_MAX) {
			LOG_ERR("request size exceeds the limit: %d", RES_DLEN_MAX);
			code = EC_RET_ERR_INV_SIZE;
			dat_size = 0;
			break;
		}

		flag = RES_META_F_TIMESTAMP;

		/* pre-alloc 32bits timestamp */
		for (i = 4; i < dat_size; i++) {
			res_data[i] = i;
		}

		break;
	default:
		code = EC_RET_ERR_INV_FUNC;
	}

	lom_mgmt_i2c_response_ready(lom_mgmt_dev, code, dat_size, flag);

	task->req = NULL;
	task->res = NULL;

	return 0;
}

static void wait_hwdata_ready(uint32_t normal_period)
{
	while (true) {
		if (smchost_is_system_in_cs()) {
			k_sleep(K_SECONDS(CPU_TEMP_CS_ACCESS_PERIOD_SEC));
		} else {
			k_msleep(normal_period);
		}

		if (hwmon_data != NULL) {
			LOG_DBG("hwmon_data initialized to %p\n", hwmon_data);
			break;
		}
	}
}

#if 1
struct vwi_signal_info {
	char *name;
	uint8_t id;
};

static struct vwi_signal_info vwi_managed_sigs[] =
{
	{"SLP_S3",        0  },
	{"SLP_S4",        0  },
	{"SLP_S5",        0  },
	{"OOB_RST_WARN",  0 },
	{"PLTRST",        0 },
	{"SUS_STAT",      0          },
	{"NMIOUT",        0          },
	{"SMIOUT",        0          },
	{"HOST_RST_WARN", 0},
	{"SLP_A",         0          },
	{"SUS_PWRDN_ACK", 0          },
	{"SUS_WARN",      0 },
	{"SLP_WLAN",      0          },
	{"SLP_LAN",       0          },
	{"HOST_C10",      0          },
	{"DNX_WARN",      0     },

	{"PME", 0},
	{"WAKE", 0},
	{"OOB_RST_ACK", 0},
	{"SLV_BOOT_STS", 0},
	{"ERR_NON_FATAL", 0},
	{"ERR_FATAL", 0},
	{"SLV_BOOT_DONE", 0},
	{"HOST_RST_ACK", 0},
	{"RST_CPU_INIT", 0},
	{"SMI", 0},
	{"SCI", 0},
	{"DNX_ACK", 0},
	{"SUS_ACK", 0},
};
#endif

static void espi_event_record_vwi(const struct device *dev,
	struct espi_callback *cb, struct espi_event event)
{
	if (event.evt_details > ESPI_VWIRE_SIGNAL_SUS_ACK) {
		LOG_INF(">> VWI_EVT: SIG[%2d] %13s: %x", event.evt_details, "UNKNOWN", event.evt_data);
		return;
	}

	uint8_t acpi_sta = pwrseq_system_state();

	if (event.evt_details == ESPI_VWIRE_SIGNAL_SLP_S5 && event.evt_data == 0) {
		//LOG_DBG(">> VWI_EVT: SIG[%2d] %13s: %x (ACPI %d)", event.evt_details,
		//	vwi_managed_sigs[event.evt_details].name, event.evt_data, sta);
		if (acpi_sta == SYSTEM_S0_STATE) {
			LOG_INF(">> [HOST Soft Off]");
			host_event_put(HOST_EVENT_SOFT_DN);
			return;
		}
	}
	else if (event.evt_details == ESPI_VWIRE_SIGNAL_SLP_S3 && event.evt_data == 1) {
		//LOG_DBG(">> VWI_EVT: SIG[%2d] %13s: %x (ACPI %d)", event.evt_details,
		//	vwi_managed_sigs[event.evt_details].name, event.evt_data, sta);
		if (acpi_sta == SYSTEM_G3_STATE) {
			LOG_INF(">> [HOST Power Up]");
			host_event_put(HOST_EVENT_POWER_UP);
			return;
		}
		else if (acpi_sta > SYSTEM_S0_STATE) {
			LOG_INF(">> [HOST Soft Up.(ACPI %d)]", acpi_sta);
			host_event_put(HOST_EVENT_SOFT_UP);
			return;
		}
	}
	else if (event.evt_details == ESPI_VWIRE_SIGNAL_PLTRST && event.evt_data) {
		//LOG_INF(">> PLT RST UP, ACPI %d", pwrseq_system_state());
		if (acpi_sta == SYSTEM_S0_STATE) {
			LOG_INF(">> [Host Reboot]");
			host_event_put(HOST_EVENT_REBOOT);
			return;
		}
	}
	else if (event.evt_details == ESPI_VWIRE_SIGNAL_HOST_RST_WARN && event.evt_data == 1) {
		LOG_INF(">> [HOST RST (ACPI %d)]", pwrseq_system_state());
		host_event_put(HOST_EVENT_HOST_RESET);
		return;
	}
	else if (event.evt_details == ESPI_VWIRE_SIGNAL_DNX_WARN && event.evt_data == 1) {
		LOG_INF(">> [DNX WARN]");
		host_event_put(HOST_EVENT_DNX_WARN);
		return;
	}

	LOG_INF(">> VWI_EVT: SIG[%2d] %13s: %x", event.evt_details,
		vwi_managed_sigs[event.evt_details].name, event.evt_data);
}

static void espi_event_record_bus(const struct device *dev,
	struct espi_callback *cb, struct espi_event event)
{
	LOG_INF(">> BUS_EVT: signal BUS_RST, data %x", event.evt_data);
	host_event_put(HOST_EVENT_BUS_RESET);
}

static struct espi_callback espi_evt_cb_bus;
static struct espi_callback espi_evt_cb_vwi;
static int event_monitor_inited = 0;

void init_espi_event_monitor(void)
{
	if (!device_is_ready(espi_dev)) {
		LOG_ERR("%s: device not ready.", espi_dev->name);
		return;
	}

	LOG_ERR("%s: device ready.", espi_dev->name);

	espi_init_callback(&espi_evt_cb_bus, espi_event_record_bus, ESPI_BUS_RESET);
	espi_init_callback(&espi_evt_cb_vwi, espi_event_record_vwi, ESPI_BUS_EVENT_VWIRE_RECEIVED);

	espi_add_callback(espi_dev, &espi_evt_cb_bus);
	espi_add_callback(espi_dev, &espi_evt_cb_vwi);

	event_monitor_inited = 1;
}

static void pwrbtn_event_handler(uint8_t pwrbtn_sts)
{
	if (pwrbtn_sts == 1)
		return;

	uint8_t acpi_sta = pwrseq_system_state();

	if (acpi_sta != SYSTEM_G3_STATE && acpi_sta != SYSTEM_S5_STATE) {
		LOG_INF(">> Button Power Down");
		host_event_put(HOST_EVENT_PWRBTN_DN);
	}
	else {
		LOG_INF(">> Button Power UP");
		host_event_put(HOST_EVENT_PWRBTN_UP);
	}
}

static void rstbtn_event_handler(uint8_t rstbtn_sts)
{
	if (rstbtn_sts == 0) {
		LOG_INF(">> Button Reset Pressed");
		host_event_put(HOST_EVENT_RSTBTN_PRESSED);
	}
	else {
		LOG_INF(">> Button Reset Released");
		host_event_put(HOST_EVENT_RSTBTN_RELEASED);
	}
}

void init_gpio_event_monitor(void)
{
	pwrbtn_register_handler(pwrbtn_event_handler);
	rstbtn_register_handler(rstbtn_event_handler);

	return;
}

void lom_mgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

	init_espi_event_monitor();
	init_gpio_event_monitor();

	lom_mgmt_dyn_sensor_table_init();

	wait_hwdata_ready(normal_period);

	lom_mgmt_i2c_set_callbacks(lom_mgmt_dev, &callbacks);

	while (true) {
		k_sem_take(&lom_mgmt_sem, K_FOREVER);

		lom_mgmt_handle_request(&lom_mgmt_task);
	}
}
