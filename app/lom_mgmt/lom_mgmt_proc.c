/*
 * Copyright (c) 2019 Intel Corporation.
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>

#include "board_config.h"
#include "smchost.h"
#include "hwmon.h"
#ifdef CONFIG_BOARD_MEC172X_ADL_N_CP
#include "hwmon_cp.h"
#endif

#include "pwrplane.h"
#include "pwrbtnmgmt.h"
#include "espi_hub.h"
#include "task_handler.h"
#include "rstbutton.h"
#include "postcodemgmt.h"

#include "lom_mgmt_proc_inc.h"
#include "host_event.h"
#include "postcode.h"
#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
#include "lom_pwrcycle.h"
#endif

LOG_MODULE_REGISTER(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define CPU_TEMP_CS_ACCESS_PERIOD_SEC 8U

#if defined(CONFIG_BOARD_MEC172X_ADL_N_CP) && !defined(CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE)
#error "Error: CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE must be enabled"
#endif

extern struct hwmon_sram *hwmon_data;

#define SENS_DTYPE_INTEG 0
#define SENS_DTYPE_FLOAT 1

#define HWMON_SDATA_MUL_OFF 7

struct hwmon_sram_entry_desc {
	uint16_t          entry_idx;
	enum sensor_types sens_type;
	uint8_t           sens_id;
};

#ifdef CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE
struct sensor_record {
	uint8_t  info; /* b[7:6]:type, b[5:0]: multiplier */
	uint8_t  sens_id;
	uint16_t value;
} __attribute__((__packed__));

#define SENS_INFO_MUL_BIT 0
#define SENS_INFO_TYP_BIT 6 /* data type(2bits): 0: integer, 1: float */

#define SENS_INFO_MUL_MASK 0x3F

#define SRD_SENS_ID(srd) (srd->sens_id)

#define SET_SRD_SENS_ID(srd, val)		\
	do {					\
		srd->sens_id = val;		\
	} while (0)
#else
struct sensor_record {
	uint8_t info; /* b[7]:type, b[6]:multiplier, b[5:0]: sensor id */
	uint16_t value;
} __attribute__((__packed__));

#define SENS_INFO_MUL_BIT 6
#define SENS_INFO_TYP_BIT 7 /* data type(1bits): 0: integer, 1: float */

#define SENS_INFO_MUL_MASK 0x1

#define SRD_SENS_ID(srd) (srd->info & 0x3F)

#define SET_SRD_SENS_ID(srd, val)		\
	do {					\
		srd->info |= val;		\
	} while (0)
#endif

#define SENSOR_RECORD_SIZE (sizeof(struct sensor_record))

#define LOM_SENSOR_MAX 256

#ifdef CONFIG_BOARD_MEC172X_ADL_N_CP

#define DYN_SENSOR_HWMON_IDX_BASE 43

/*
  The new added sensors always be stored in contiguous memory locations and at the end,
  so the sensor_id could be calculated by sequence
*/
static struct hwmon_sram_entry_desc hwmon_entries[LOM_SENSOR_MAX] = {
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
#else
static struct hwmon_sram_entry_desc hwmon_entries[LOM_SENSOR_MAX] = {
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
#endif


static const struct device *lom_mgmt_dev = DEVICE_DT_GET(DT_NODELABEL(lom_mgmt));

K_SEM_DEFINE(lom_mgmt_sem, 0, 1);

struct lom_mgmt_task {
	struct lom_mgmt_req *req;
	struct lom_mgmt_res *res;
};

static struct lom_mgmt_task lom_mgmt_task;

struct func_ret_info
{
	uint8_t code;
	union {
		uint16_t data_size;
		uint16_t fail_code; /* when code == EC_RET_ERR_FAIL, this field
				       contains the system error */
	};
	uint8_t flag; /* currently, only support timestamp flag */
};

/* sys_error is negative or zero */
#define SET_RET_CODE(pfri, err_code, sys_error)		\
	do {						\
		(pfri)->code = err_code;		\
		(pfri)->fail_code = -sys_error;		\
	} while (0)



#if defined(CONFIG_LOM_MGMT_FUNC_POWER_CTRL) || defined(CONFIG_LOM_MGMT_FUNC_HOST_EVENT) || \
	defined(CONFIG_LOM_MGMT_FUNC_POSTCODE)
static const struct device *const espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

#endif

#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL

static int slp_sig_PLTRST;
static int in_force_down;

struct pwrctrl_work_data
{
	struct k_work work_item;
        uint8_t act;
	int     sec;
};

#define HOST_NORMALDOWN_WAIT_TIME_MS CONFIG_HOST_GRACEFUL_SHUTDOWN_WAIT_TIME
#define HOST_FORCEDOWN_WAIT_TIME_MS 7200

#define WAIT_SIG_SLEEP_TIME_MS 10
#define MS_TIMEOUT_TO_CNT(t)						\
	(((t) + WAIT_SIG_SLEEP_TIME_MS - 1) / WAIT_SIG_SLEEP_TIME_MS)

#define WORK_RET_OK      0
#define WORK_RET_TIMEOUT 1
#define WORK_RET_QUIT    2

static struct pwrctrl_work_data pwrctrl_work_data;

#endif

#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
static uint16_t boot_cycle_count = 0;
#endif

static uint8_t acpi_state[] = {
	6, /* Hard Off */
	1, /* S0 */
	3, /* S3 */
	4, /* S4 */
	5, /* S5 */
};

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
		LOG_ERR("Invalid Request!!");
		return -1;
	}

	k_sem_give(&lom_mgmt_sem); /* wakeup handler */

	return 0;
}

static struct lom_mgmt_i2c_callbacks callbacks = {
	lom_mgmt_i2c_request,
	lom_mgmt_i2c_cancel,
};

void avail_resource_set(int bit, int val)
{
	lom_mgmt_i2c_set_avail_res(lom_mgmt_dev, bit, val);
}

#ifdef CONFIG_BOARD_MEC172X_ADL_N_CP
static void lom_mgmt_sw_sensor_table_init(void)
{
	int start_idx;
	int used_max_sens_id = 0;
	int entry_idx = DYN_SENSOR_HWMON_IDX_BASE;
	int i;

	for (i = 0; i < LOM_SENSOR_MAX; i++) {
		if (hwmon_entries[i].entry_idx == 0xFFFF) {
			break;
		}
		if (hwmon_entries[i].sens_id > used_max_sens_id) {
			used_max_sens_id = hwmon_entries[i].sens_id;
		}
	}
	start_idx = i;

	for (i = 0; i < SW_THERMAL_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_temp;
		hwmon_entries[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<TEMP> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_THERMAL_SENSOR_NUM;

	for (i = 0; i < SW_VOLTAGE_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_in;
		hwmon_entries[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<VOLT> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_VOLTAGE_SENSOR_NUM;

	for (i = 0; i < SW_CURRENT_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_curr;
		hwmon_entries[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<CURR> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_CURRENT_SENSOR_NUM;

	for (i = 0; i < SW_POWER_SENSOR_NUM; i++, entry_idx++) {
		hwmon_entries[start_idx + i].entry_idx = entry_idx;
		hwmon_entries[start_idx + i].sens_type = hwmon_power;
		hwmon_entries[start_idx + i].sens_id   = ++used_max_sens_id;

		LOG_DBG_SENS("<POWR> SENS[%02d]: { %d, %d, %3d }", start_idx +i,
			hwmon_entries[start_idx + i].entry_idx,
			hwmon_entries[start_idx + i].sens_type,
			hwmon_entries[start_idx + i].sens_id);
	}
	start_idx += SW_POWER_SENSOR_NUM;

	hwmon_entries[start_idx].entry_idx = 0xFFFF;

	LOG_DBG_SENS("<LAST> SENS[%02d]: { -1, %d, %3d }", start_idx,
		hwmon_entries[start_idx].sens_type,
		hwmon_entries[start_idx].sens_id);
}
#endif

static inline void fill_one_sensor(struct sensor_record *srd,
	const struct hwmon_sram_entry_desc * ent)
{
	uint16_t *sram = (uint16_t *)hwmon_data;
	uint16_t offset = (ent->entry_idx * 32) / 2; /* offset in 2bytes unit */

	uint8_t mul = (uint8_t)(sram[offset + HWMON_SDATA_MUL_OFF] & SENS_INFO_MUL_MASK);

	srd->info = 0;
	if (ent->sens_type == hwmon_fan) {
		srd->info = (SENS_DTYPE_INTEG << SENS_INFO_TYP_BIT) | 0; /* fan no multiplier */
	}
	else {
		srd->info = (SENS_DTYPE_FLOAT << SENS_INFO_TYP_BIT) | (mul << SENS_INFO_MUL_BIT);
	}

	SET_SRD_SENS_ID(srd, ent->sens_id);

	srd->value = htons(sram[offset]);
}

static int do_get_sensors(uint8_t *res_data, struct func_ret_info* fri)
{
	struct sensor_record * srd;
	uint16_t data_off = 0;

	for (int i = 0; hwmon_entries[i].entry_idx != 0xFFFF; i++) {
		if (data_off + SENSOR_RECORD_SIZE > RES_DLEN_MAX) {
			LOG_ERR("payload size exceeds the limit: %d", RES_DLEN_MAX);
			SET_RET_CODE(fri, EC_RET_ERR_FAIL, -ENOSPC);
			return -1;
		}

		srd = (struct sensor_record *)&(res_data[data_off]);

		fill_one_sensor(srd, &hwmon_entries[i]);

		LOG_DBG_SENS("SENS@hwmon[%03d] 0x%02x %3d 0x%04x", hwmon_entries[i].entry_idx,
			srd->info, SRD_SENS_ID(srd), ntohs(srd->value));

		data_off += SENSOR_RECORD_SIZE;
	}

	fri->data_size = data_off;

	return 0;
}

static int do_get_events(uint8_t *res_data, struct func_ret_info* fri)
{
#ifdef CONFIG_LOM_MGMT_FUNC_HOST_EVENT
	uint8_t * data = &res_data[TIMESTAMP_SZ]; /* reserve for timestamp */

	int ret = host_event_get(data, RES_DLEN_MAX - TIMESTAMP_SZ, &fri->data_size);
	if (ret < 0) {
		LOG_ERR("Get events failed, ret %d", ret);
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, ret);
		return -1;
	}

	if (fri->data_size) {
		fri->data_size += TIMESTAMP_SZ; /* reserve 4bytes for timestamp */
		fri->flag = RES_META_F_TIMESTAMP;
	}
#else
	fri->code = EC_RET_ERR_NOT_IMPL;
#endif
	return 0;
}

static int do_get_postcode(uint8_t *res_data, struct func_ret_info* fri)
{
#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
	uint8_t *data = &res_data[TIMESTAMP_SZ]; /* reserve 4bytes for timestamp */

	int ret = postcode_get(data, RES_DLEN_MAX - TIMESTAMP_SZ, &fri->data_size);
	if (ret < 0) {
		LOG_ERR("Get postcode failed, ret %d", ret);
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, ret);
		return -1;
	}

	if (fri->data_size) {
		fri->data_size += TIMESTAMP_SZ;
		fri->flag = RES_META_F_TIMESTAMP;
	}
#else
	fri->code = EC_RET_ERR_NOT_IMPL;
#endif

	return 0;
}

static int do_get_fru_cache(uint8_t *data, struct func_ret_info* fri)
{
#ifdef CONFIG_LOM_MGMT_FUNC_FRU
	uint16_t fru_size = 0;

	const uint8_t *fru_cache = board_fru_data(&fru_size);

	if (fru_cache == NULL || fru_size == 0) {
		LOG_ERR("FRU not present");
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, -ENODEV);
		return -1;
	}

	LOG_DBG_APP("FRU size %u", fru_size);

	if (fru_size > RES_DLEN_MAX) {
		LOG_ERR("Too large FRU %d", fru_size);
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, -ENOSPC);
		return -1;
	}

	memcpy(data, fru_cache, fru_size);

	fri->data_size = fru_size;
#else
	fri->code = EC_RET_ERR_NOT_IMPL;
#endif

	return 0;
}

#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
static void pwrctrl_do_up(void)
{
	LOG_DBG_APP(">> Do UP");

	gpio_write_pin(PM_PWRBTN, 0);
	k_msleep(150);
	gpio_write_pin(PM_PWRBTN, 1);

	LOG_DBG_APP(">> Do UP end");
}

static void pwrctrl_do_shutdown(void)
{
	LOG_DBG_APP(">> Do Normal Down");

	gpio_write_pin(PM_PWRBTN, 0);
	k_msleep(150);
	gpio_write_pin(PM_PWRBTN, 1);

	LOG_DBG_APP(">> Do Normal Down end");
}

static int wait_sig_value(volatile int *sig, int exp_val, uint32_t timeout)
{
	uint16_t loop_cnt = MS_TIMEOUT_TO_CNT(timeout);

	while (loop_cnt && *sig != exp_val) {
		k_msleep(WAIT_SIG_SLEEP_TIME_MS);
		loop_cnt--;
	}

	return ((*sig == exp_val) ? WORK_RET_OK : WORK_RET_TIMEOUT);
}

static int wait_sig_value_with_init(volatile int *sig, int set_val, int exp_val,
	uint32_t timeout)
{
	*sig = set_val;

	return wait_sig_value(sig, exp_val, timeout);
}

static inline void pwrctrl_forcedown_post(void)
{
	if (in_force_down) {
		gpio_write_pin(PM_PWRBTN, 1);
		in_force_down = 0;
	}
}

static void pwrctrl_do_force_down(void)
{
	int ret;

	LOG_DBG_APP(">> Do Force Down");

#ifdef CONFIG_ATTEMPT_GRACEFUL_SHUTDOWN
	slp_sig_PLTRST = -1;

	pwrctrl_do_shutdown();

	ret = wait_sig_value(&slp_sig_PLTRST, 0, HOST_NORMALDOWN_WAIT_TIME_MS);
	if (ret == WORK_RET_QUIT) {
		LOG_DBG_APP(">> Normal Down wait quit");
		return;
	}

	if (ret == WORK_RET_TIMEOUT) {
		LOG_DBG_APP(">> Normal Down failed, try Force Down");
#endif

		gpio_write_pin(PM_PWRBTN, 0);

		ret = wait_sig_value_with_init(&in_force_down, 1, 0, HOST_FORCEDOWN_WAIT_TIME_MS);
		if (ret == WORK_RET_QUIT) {
			LOG_DBG_APP(">> Force Down Wait quit");
			pwrctrl_forcedown_post();
			return;
		}

		if (ret == WORK_RET_TIMEOUT) {
			LOG_ERR(">> Do Force Down Failed");
			pwrctrl_forcedown_post();
			return;
		}
#ifdef CONFIG_ATTEMPT_GRACEFUL_SHUTDOWN
	}
#endif

	LOG_DBG_APP(">> Do Force Down end");
}

static void pwrctrl_do_hard_reset(void)
{
	LOG_DBG_APP(">> Do Hard Reset");

	gpio_write_pin(SOC_RSTBTN_N, 0);
	k_msleep(20);
	gpio_write_pin(SOC_RSTBTN_N, 1);

	LOG_DBG_APP(">> Do Hard Reset end");
}

/*
 * The array index must match the 'enum lom_mgmt_power_ctrl_act'.
 *
 * PWC_POWER_CYCLE is handled by lom_pwrcycle.c, which confirms the host
 * actually reached S5 before restarting it. lom_pwrcycle_start() returns
 * immediately; the sequence runs on its own thread.
 */
static void (*pwrctrl_funcs[])(void) = {
	NULL,
	pwrctrl_do_up,
	pwrctrl_do_shutdown,
	pwrctrl_do_hard_reset,
	pwrctrl_do_force_down,
	lom_pwrcycle_start,
};

static void pwrctrl_worker(struct k_work *work)
{
	struct pwrctrl_work_data *data = CONTAINER_OF(work, struct pwrctrl_work_data, work_item);

	if (pwrctrl_funcs[data->act]) {
		pwrctrl_funcs[data->act]();
	}
	else {
		LOG_WRN("pwrctrl_worker fail quit");
	}
}
#endif

static int do_power_ctrl(uint8_t* req, struct func_ret_info* fri)
{
#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
	uint8_t pwr_state = pwrseq_system_state();
	int valid_request = 0;

	switch (req[0]) {
	case PWC_UP:
		if (pwr_state == SYSTEM_S5_STATE || pwr_state == SYSTEM_S4_STATE) {
			valid_request = 1;
		}
		break;
	case PWC_SHUTDOWN:
	case PWC_HARD_RESET:
	case PWC_FORCE_DOWN:
	case PWC_POWER_CYCLE:
		if (pwr_state == SYSTEM_S0_STATE) {
			valid_request = 1;
		}
		break;
	default:
		LOG_ERR("Unsupported PWC action %d", req[0]);
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, -ENOTSUP);
		return -1;
	}

	if (!valid_request) {
		LOG_ERR("Invalid PWC action <%d> on pwr_stat %d. Ignore", req[0],
			pwr_state);
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, -EINVAL);
		return -1;
	}

	uint32_t status = k_work_busy_get(&pwrctrl_work_data.work_item);
	if (status & K_WORK_RUNNING) {
		LOG_DBG_APP("PwrCtrl worker is busy");
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, -EBUSY);
		return -1;
	}

	pwrctrl_work_data.act = req[0];
	pwrctrl_work_data.sec = req[1] * 10;

	k_work_submit(&pwrctrl_work_data.work_item);
#else
	fri->code = EC_RET_ERR_NOT_IMPL;
#endif

	return 0;
}

static int do_report_lom_ip(uint8_t* req_data, struct func_ret_info* fri)
{
#ifdef CONFIG_LOM_MGMT_FUNC_REPORT_LOM_IP
	uint8_t family = req_data[0];
	uint8_t prefix = req_data[1];
	int size = 0;

	if (family == LOM_IP_FAMILY_V4) {
		size = 6;
	}
	else if (family == LOM_IP_FAMILY_V6) {
		size = 18;
	}
	else {
		LOG_ERR("Unsupported address family: %d", family);
		SET_RET_CODE(fri, EC_RET_ERR_FAIL, -EINVAL);
		return -1;
	}

#if CONFIG_LOM_MGMT_PROC_DBG_APP
	if (size) {
		LOG_HEXDUMP_INF(req_data, size, "REPORT_LOM_IP:");
	}
#endif

	set_lom_ip(family, prefix, &req_data[2], size - 2);
#else
	fri->code = EC_RET_ERR_NOT_IMPL;
#endif

	return 0;
}

#ifdef LOM_MGMT_PROTO_STRESS_TESTING
static int do_test_l3(uint8_t* req, uint8_t*res, struct func_ret_info* fri)
{
	int test_dlen = ((uint16_t)req[0] << 8) | req[1]; /* test_dlen include the timestamp */

	LOG_DBG_APP("TestL3 size %d", test_dlen);

	if (test_dlen < 4 || test_dlen > RES_DLEN_MAX) {
		SET_RET_CODE(fri, EC_RET_ERR_INV_SIZE, 0);
		LOG_ERR("Invalid test size %d, shoule between 4 and %d", test_dlen, RES_DLEN_MAX);
		return -1;
	}

	/* reserve 4bytes for timestamp */
	for (int i = 4; i < test_dlen; i++) {
		res[i] = i;
	}

	fri->data_size = test_dlen;
	fri->flag = RES_META_F_TIMESTAMP;

	return 0;
}
#endif

static int do_get_acpi(uint8_t* data, struct func_ret_info* fri)
{
	uint8_t s = pwrseq_system_state();
	data[0] = acpi_state[s];

	fri->data_size = 1;

	return 0;
}

static int do_get_ids(uint8_t *res_data, struct func_ret_info* fri)
{
#ifdef CONFIG_LOM_MGMT_FUNC_GET_SYS_IDS
	struct sys_ids ids;

	get_sbl_version(&ids.sbl);
	ids.board_id = get_board_id();
	ids.bom_id   = get_bom_id();

	memcpy(res_data, &ids, sizeof(ids));

	fri->data_size = sizeof(ids);

#ifdef CONFIG_LOM_MGMT_PROC_DBG_APP
	LOG_HEXDUMP_INF(res_data, fri->data_size, "GET_IDS:");
#endif
#else
	fri->code = EC_RET_ERR_NOT_IMPL;
#endif

	return 0;
}

static int lom_mgmt_handle_request(struct lom_mgmt_task *task)
{
	if (task->req == NULL || task->res == NULL) {
		LOG_ERR("No Request!");
		return -1;
	}

	LOG_DBG_APP("Processing F<%d>", task->req->func);

	uint8_t *req_data = task->req->data;
	uint8_t *res_data = task->res->data;
	struct func_ret_info fri = { .code = EC_RET_OK, .data_size = 0, .flag = 0 };

	switch (task->req->func) {
	case FUNC_GET_ID:
		do_get_ids(res_data, &fri);
		break;
	case FUNC_POWER_CTRL:
		do_power_ctrl(req_data, &fri);
		break;
	case FUNC_GET_ACPI_POWER_STATE:
		do_get_acpi(res_data, &fri);
		break;
	case FUNC_GET_SENSORS:
		do_get_sensors(res_data, &fri);
		break;
	case FUNC_GET_FRU:
		do_get_fru_cache(res_data, &fri);
		break;
	case FUNC_GET_FAULT_CODE:
		fri.code = EC_RET_ERR_NOT_IMPL;
		break;
	case FUNC_GET_EVENTS:
		do_get_events(res_data, &fri);
		break;
	case FUNC_GET_POSTCODE:
		do_get_postcode(res_data, &fri);
		//LOG_HEXDUMP_ERR(res_data, fri.data_size, "postcode DUMP");
		break;
	case FUNC_REPORT_LOM_IP:
		do_report_lom_ip(req_data, &fri);
		break;
#ifdef LOM_MGMT_PROTO_STRESS_TESTING
	case FUNC_TEST_L3:
		do_test_l3(req_data, res_data, &fri);
		break;
#endif
	default:
		fri.code = EC_RET_ERR_INV_FUNC;
		LOG_ERR("Unknown func %d", task->req->func);
	}

	lom_mgmt_i2c_response_ready(lom_mgmt_dev, fri.code, fri.data_size, fri.flag);

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
			LOG_INF("hwmon_data is available at %p\n", hwmon_data);
			break;
		}
	}
}

#if defined(CONFIG_LOM_MGMT_FUNC_POWER_CTRL) || defined(CONFIG_LOM_MGMT_FUNC_HOST_EVENT) || \
	defined(CONFIG_LOM_MGMT_FUNC_POSTCODE)

#ifdef CONFIG_LOM_MGMT_PROC_DBG_EVENT
struct vwi_signal_info {
	char *name;
};

static struct vwi_signal_info vwi_managed_sigs[] =
{
	{"SLP_S3",        }, /* 00 */
	{"SLP_S4",        }, /* 01 */
	{"SLP_S5",        }, /* 02 */
	{"OOB_RST_WARN",  }, /* 03 */
	{"PLTRST",        }, /* 04 */
	{"SUS_STAT",      }, /* 05 */
	{"NMIOUT",        }, /* 06 */
	{"SMIOUT",        }, /* 07 */
	{"HOST_RST_WARN", }, /* 08 */
	{"SLP_A",         }, /* 09 */
	{"SUS_PWRDN_ACK", }, /* 10 */
	{"SUS_WARN",      }, /* 11 */
	{"SLP_WLAN",      }, /* 12 */
	{"SLP_LAN",       }, /* 13 */
	{"HOST_C10",      }, /* 14 */
	{"DNX_WARN",      }, /* 15 */

	{"PME",           }, /* 16 */
	{"WAKE",          }, /* 17 */
	{"OOB_RST_ACK",   }, /* 18 */
	{"SLV_BOOT_STS",  }, /* 19 */
	{"ERR_NON_FATAL", }, /* 20 */
	{"ERR_FATAL",     }, /* 21 */
	{"SLV_BOOT_DONE", }, /* 22 */
	{"HOST_RST_ACK",  }, /* 23 */
	{"RST_CPU_INIT",  }, /* 24 */
        {"SMI",           }, /* 25 */
        {"SCI",           }, /* 26 */
        {"DNX_ACK",       }, /* 27 */
        {"SUS_ACK",       }, /* 28 */

	{"SLV_GPIO_0",    }, /* 29 */
	{"SLV_GPIO_1",    }, /* 30 */
	{"SLV_GPIO_2",    }, /* 31 */
	{"SLV_GPIO_3",    }, /* 32 */
	{"SLV_GPIO_4",    }, /* 33 */
	{"SLV_GPIO_5",    }, /* 34 */
	{"SLV_GPIO_6",    }, /* 35 */
	{"SLV_GPIO_7",    }, /* 36 */
	{"SLV_GPIO_8",    }, /* 37 */
	{"SLV_GPIO_9",    }, /* 38 */
	{"SLV_GPIO_10",   }, /* 39 */
	{"SLV_GPIO_11",   }, /* 40 */
};
#endif


/*
 * PLTRST, SLP_A, SLP_S5, SLP_S4, SLP_S3, SLP_WLAN = 1 : System Power UP
 * PLTRST, SLP_A, SLP_S5, SLP_S4, SLP_S3, SLP_WLAN = 0 : System Power Down
 * SUS_PWRDN_ACK = 0: Power UP; = 1: Power Down
 *
 * In DN process, the PLTRST = 0, is the first signal
 * In UP process, the PLTRST = 1, is the last signal
 *
 * (host_event, power control, postcode all require this function)
 */
static void espi_vwire_monitor(const struct device *dev, struct espi_callback *cb,
	struct espi_event event)
{
	if (event.evt_details > ESPI_VWIRE_SIGNAL_SUS_ACK) {
		LOG_DBG_EVENT(">> VWI_EVT: SIG[%2d] %13s: %x",
			event.evt_details, "!UNKNOWN!", event.evt_data);
		return;
	}

	uint8_t pwr_sta = pwrseq_system_state();

	LOG_DBG_EVENT(">> VWI_EVT: SIG[%2d] %13s: %x (PWRS:%d)", event.evt_details,
		vwi_managed_sigs[event.evt_details].name, event.evt_data, pwr_sta);

	switch (event.evt_details) {
	case ESPI_VWIRE_SIGNAL_SLP_WLAN:
		if (event.evt_data == 0) {
#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
			pwrctrl_forcedown_post();
#endif
		}
		break;
	case ESPI_VWIRE_SIGNAL_SLP_S5:
		if (event.evt_data == 0) { /* power off sequence */
			if (pwr_sta == SYSTEM_S0_STATE) {
				LOG_DBG_EVENT(">> [HOST Soft Off]");
				host_event_put(HOST_EVENT_SOFT_DN);
			}
		}
		break;
	case ESPI_VWIRE_SIGNAL_SLP_S3:
		if (event.evt_data == 1) { /* power on sequence */
			if (pwr_sta == SYSTEM_G3_STATE) {
				LOG_DBG_EVENT(">> [HOST Power Up]");
				host_event_put(HOST_EVENT_POWER_UP);
#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
				boot_cycle_count++;
#endif
			}
			else if (pwr_sta == SYSTEM_S4_STATE || pwr_sta == SYSTEM_S5_STATE) {
				LOG_DBG_EVENT(">> [HOST Soft Up]");
				host_event_put(HOST_EVENT_SOFT_UP);
#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
				boot_cycle_count++;
#endif
			}
		}
		break;
	case ESPI_VWIRE_SIGNAL_PLTRST:
#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
		slp_sig_PLTRST = event.evt_data;
#endif
		if (event.evt_data) {
			if (pwr_sta == SYSTEM_S0_STATE) {
				LOG_DBG_EVENT(">> [Host Reboot]");
				host_event_put(HOST_EVENT_REBOOT);
#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
				boot_cycle_count++;
#endif
			}
		}
		else {
			/*
			 * Some systems take a long time to shut down,
			 * causing the process to enter a forced shutdown procedure.
			 * The power button needs to be released(cancel the forcedown) to
			 * prevent the host from restarting.
			 */
#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
			if (in_force_down) {
				LOG_WRN(">> Normal shutdown occurs in ForceDown!");
				pwrctrl_forcedown_post();
			}
#endif
		}
		break;
	case ESPI_VWIRE_SIGNAL_HOST_RST_WARN:
		if (event.evt_data == 1) {
			LOG_DBG_EVENT(">> [HOST RST]");
			host_event_put(HOST_EVENT_HOST_RESET);
		}
		break;
	case ESPI_VWIRE_SIGNAL_DNX_WARN:
		if (event.evt_data == 1) {
			LOG_DBG_EVENT(">> [DNX WARN]");
			host_event_put(HOST_EVENT_DNX_WARN);
		}
		break;
	default:
		break;
	}
}

static void espi_bus_reset_monitor(const struct device *dev, struct espi_callback *cb,
	struct espi_event event)
{
	LOG_DBG_EVENT("eSPI BUS reset %d", event.evt_data);

	host_event_put(HOST_EVENT_BUS_RESET);
}

static struct espi_callback espi_vwi_cb;
static struct espi_callback espi_rst_cb;

void init_espi_event_monitor(void)
{
	if (!device_is_ready(espi_dev)) {
		LOG_ERR("%s: device not ready.", espi_dev->name);
		return;
	}

	espi_init_callback(&espi_rst_cb, espi_bus_reset_monitor, ESPI_BUS_RESET);
	espi_init_callback(&espi_vwi_cb, espi_vwire_monitor, ESPI_BUS_EVENT_VWIRE_RECEIVED);

	espi_add_callback(espi_dev, &espi_rst_cb);
	espi_add_callback(espi_dev, &espi_vwi_cb);
}

static void pwrbtn_event_handler(uint8_t pwrbtn_sts)
{
	if (pwrbtn_sts == 0) {
		LOG_DBG_EVENT("Power Button Pressed");
		host_event_put(HOST_EVENT_PWRBTN_DN);
	}
	else {
		LOG_DBG_EVENT("Power Button Released");
		host_event_put(HOST_EVENT_PWRBTN_UP);
	}
}

static void rstbtn_event_handler(uint8_t rstbtn_sts)
{
	if (rstbtn_sts == 0) {
		LOG_DBG_EVENT("Reset Button Pressed");
		host_event_put(HOST_EVENT_RSTBTN_PRESSED);
	}
	else {
		LOG_DBG_EVENT("Reset Button Released");
		host_event_put(HOST_EVENT_RSTBTN_RELEASED);
	}
}

void init_gpio_event_monitor(void)
{
	pwrbtn_register_handler(pwrbtn_event_handler);
	rstbtn_register_handler(rstbtn_event_handler);

	return;
}
#endif

#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
static void postcode_disp_event_handler(uint16_t code)
{
	postcode_add(code, boot_cycle_count);
}

void init_postcode_disp_event_monitor(void)
{
	postcode_add_disp_event_handler(postcode_disp_event_handler);

	LOG_INF("PostCode Disp Event Handler Registered");
}
#endif

void lom_mgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

#if defined(CONFIG_LOM_MGMT_FUNC_POWER_CTRL) || defined(CONFIG_LOM_MGMT_FUNC_HOST_EVENT) || \
	defined(CONFIG_LOM_MGMT_FUNC_POSTCODE)
	init_espi_event_monitor();
	init_gpio_event_monitor();
#endif

#ifdef CONFIG_LOM_MGMT_FUNC_POSTCODE
	init_postcode_disp_event_monitor();
#endif

	wait_hwdata_ready(normal_period);

#ifdef CONFIG_LOM_MGMT_FUNC_POWER_CTRL
	k_work_init(&pwrctrl_work_data.work_item, pwrctrl_worker);
#endif

#ifdef CONFIG_BOARD_MEC172X_ADL_N_CP
	lom_mgmt_sw_sensor_table_init();
#endif

#ifdef CONFIG_LOM_MGMT_LARGE_SENSOR_VALUE
	LOG_INF("Use wide data-type layout for high-range sensor data.");
#endif

	lom_mgmt_i2c_set_callbacks(lom_mgmt_dev, &callbacks);

	while (true) {
		k_sem_take(&lom_mgmt_sem, K_FOREVER);

		lom_mgmt_handle_request(&lom_mgmt_task);
	}
}
