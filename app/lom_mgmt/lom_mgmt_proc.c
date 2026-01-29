/*
 * Copyright (c) 2019 Intel Corporation.
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
#include "sci.h"

#include "pwrplane.h"
#include "pwrbtnmgmt.h"
#include "espi_hub.h"
#include "task_handler.h"
#include "rstbutton.h"
#include "postcodemgmt.h"

#include "lom_mgmt_proc_inc.h"
#include "host_event.h"
#include "postcode.h"

LOG_MODULE_REGISTER(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define VWI_SIG_DBG

#define CPU_TEMP_CS_ACCESS_PERIOD_SEC 8U

#define WAIT_SIG_SLEEP_TIME 10

static const struct device *const espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

static const struct device *fru = DEVICE_DT_GET(DT_NODELABEL(fru));

extern struct hwmon_sram *hwmon_data;

struct hwmon_sram_entry_desc {
	uint16_t offset;
	uint8_t  kind;
	uint8_t  sens_info;
};

#define HWMON_KIND_TEMP 0
#define HWMON_KIND_VOLT 1
#define HWMON_KIND_CURR 2
#define HWMON_KIND_FRPM 3

#define SENS_INFO_TYP_BIT 7
#define SENS_INFO_MUL_BIT 6

#define SENS_INFO_DEF(t, m, id) \
	((t) << SENS_INFO_TYP_BIT | ((m) << SENS_INFO_MUL_BIT) | ((id) & 0x3F))

static const struct hwmon_sram_entry_desc hwmon_entries[] = {
	/* hwmon_sdata[16]: 0x100 */
	{ 0x100, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0,  6) }, /* P12V0A        */
	{ 0x120, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0,  7) }, /* P5V0A         */
	{ 0x140, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0,  8) }, /* P3V3_ALW_ON   */
	{ 0x160, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0,  9) }, /* P1V8_ALW_ON   */
	{ 0x180, HWMON_KIND_TEMP, SENS_INFO_DEF(1, 0,  1) }, /* AmbientTemp   */
	{ 0x1a0, HWMON_KIND_TEMP, SENS_INFO_DEF(1, 0,  2) }, /* VR_Temp       */
	{ 0x1c0, HWMON_KIND_TEMP, SENS_INFO_DEF(1, 0,  3) }, /* DDR_Temp      */
	{ 0x220, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0, 10) }, /* P1V8A         */
	{ 0x240, HWMON_KIND_TEMP, SENS_INFO_DEF(1, 0,  4) }, /* CPU_Temp      */
	{ 0x260, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0, 11) }, /* VCCIN_AUX     */
	{ 0x280, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0, 12) }, /* P1V065_VDD2   */
	{ 0x2a0, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0, 13) }, /* P0V95S        */
	{ 0x2c0, HWMON_KIND_VOLT, SENS_INFO_DEF(1, 0, 14) }, /* P0V5_VDDQ     */

	{ 0x2e0, HWMON_KIND_CURR, SENS_INFO_DEF(1, 0, 17) }, /* PWR_MON       */

	/* hwmon_peci:      0x300 */
	{ 0x300, HWMON_KIND_TEMP, SENS_INFO_DEF(1, 0,  5) }, /* CPU_PECI_Temp */

	/* hwmon_fdata[4]:  0x320 */
	{ 0x320, HWMON_KIND_FRPM, SENS_INFO_DEF(0, 0, 15) }, /* Fan1_Speed    */
	{ 0x340, HWMON_KIND_FRPM, SENS_INFO_DEF(0, 0, 16) }, /* Fan2_Speed    */

	/* hwmon_pdata[4]:  0x3a0 */
	//{ 0x3a0, 0, F0, 0, "Fan1_PWM" },
	//{ 0x3c0, 0, F0, 0, "Fan2_PWM" },
	{ 0, 0, 0}
};

struct sensor_record {
	union {
		struct {
			uint8_t type:1;
			uint8_t multiplier:1;
			uint8_t id:6;
		};
		uint8_t info;
	};
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


struct pwrctrl_work_data
{
	struct k_work work_item;
        uint8_t act;
	uint8_t sec;
};

static struct pwrctrl_work_data pwrctrl_work_data;

static volatile int slp_sig_PLTRST;
static volatile int in_force_down;


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

#define FRU_HDR_SIZE 11
static uint8_t fru_hdr_title[] = "TlvInfo";

#define FRU_READ_MAX 64 /* Read FRU should not occupy cpu for long time */

#define SET_FAIL_CODE(res, err) \
	do { \
		(res)->dlen_or_error = htons(err); \
	} while (0)

static inline void fill_one_sensor(struct sensor_record *srd,
	const struct hwmon_sram_entry_desc * ent)
{
	uint16_t *sram = (uint16_t *)hwmon_data;

	srd->info  = ent->sens_info;
	if (ent->kind != HWMON_KIND_FRPM) {
		srd->info |= (uint8_t)(sram[ent->offset/2 + 7] & 0x1) << SENS_INFO_MUL_BIT;
	}

	srd->value = htons(sram[ent->offset/2]);

	//LOG_INF(">> orig: info 0x%02x", ent->sens_info);
	//LOG_INF(">> 0x%02x 0x%02x 0x%02x", srd->info, ((uint8_t*)srd)[1], ((uint8_t *)srd)[2]);
	//LOG_INF(">> val : 0x%04x", srd->value);
}

static int do_get_sensors(uint8_t *res_data, uint16_t *dat_size)
{
	struct sensor_record * srd;
	uint16_t data_off = 0;

	for (int i = 0; hwmon_entries[i].offset != 0; i++) {
		if (data_off + SENSOR_RECORD_SIZE > RES_DLEN_MAX) {
			return -1;
		}

		srd = (struct sensor_record *)&(res_data[data_off]);

		fill_one_sensor(srd, &hwmon_entries[i]);

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

static int do_get_events(uint8_t *data, uint16_t *dat_size)
{
	int off = 0;
	int ret;

	while ((ret = host_event_get(&data[off])) > 0) {
		off += ret;
	}

	*dat_size = off;

	return 0;
}

#ifdef CONFIG_POSTCODE_MONITOR
static int do_get_postcode(uint8_t *data, uint16_t *dat_size)
{
	return postcode_get(data, dat_size);
}
#endif

static uint8_t acpi_state[] = {
	6, /* Hard Off */
	1, /* S0 */
	3, /* S3 */
	4, /* S4 */
	5, /* S5 */
};


static int do_get_fru(uint8_t *data, uint16_t * dat_size)
{
	off_t offset = 0;
	int ret;

	ret = eeprom_read(fru, offset, &data[offset], FRU_HDR_SIZE);
	if (ret < 0) {
		LOG_ERR("Unable to read fru, ret %d", ret);
		return ret;
	}
	offset += FRU_HDR_SIZE;

	if (memcmp(data, fru_hdr_title, 8)) {
		LOG_ERR("Unsupport FRU format");
		return -ENXIO;
	}

	uint16_t fru_dat_len = data[9] << 8 | data[10];

	LOG_DBG("Fru data size 0x%04x", fru_dat_len);

	size_t read_size = 0;
	for (uint16_t remains = fru_dat_len; remains > 0; remains -= read_size) {
		read_size = remains > FRU_READ_MAX ? FRU_READ_MAX : remains;
		ret = eeprom_read(fru, offset, &data[offset], read_size);
		if (ret < 0) {
			LOG_ERR("Unable to read fru, ret %d", ret);
			return ret;
		}
		offset += read_size;

		if (remains - read_size)
			k_yield();
	}

	*dat_size = FRU_HDR_SIZE + fru_dat_len;

	return 0;
}

static void pwrctrl_do_up()
{
	uint8_t pwr_state = pwrseq_system_state();

	if (pwr_state == SYSTEM_S5_STATE || pwr_state == SYSTEM_S4_STATE) {
		LOG_INF(">> Do UP");

		gpio_write_pin(PM_PWRBTN, 0);
		k_msleep(150);
		gpio_write_pin(PM_PWRBTN, 1);

		LOG_INF(">> Do UP end");
	}
	else {
		LOG_INF(">> Skip UP");
	}
}

static void pwrctrl_do_shutdown()
{
	uint8_t pwr_state = pwrseq_system_state();

	if (pwr_state == SYSTEM_S0_STATE) {
		LOG_INF(">> Do Down");

		gpio_write_pin(PM_PWRBTN, 0);
		k_msleep(150);
		gpio_write_pin(PM_PWRBTN, 1);

		LOG_INF(">> Do Down end");
	}
	else {
		LOG_INF(">> Skip Down");
	}
}

static bool wait_sig_value(volatile int *sig,
	int set_val, int exp_val, uint16_t timeout)
{
	uint16_t loop_cnt = timeout / WAIT_SIG_SLEEP_TIME + ((timeout % WAIT_SIG_SLEEP_TIME) ? 1 : 0);

	*sig = set_val;

	while (*sig != exp_val && loop_cnt) {
		k_msleep(WAIT_SIG_SLEEP_TIME);
		loop_cnt--;
	}

	return (*sig == exp_val);
}

static void pwrctrl_do_force_down()
{
	uint8_t pwr_state = pwrseq_system_state();
	bool ret;

	if (pwr_state != SYSTEM_S5_STATE && pwr_state != SYSTEM_S4_STATE) {
		LOG_INF(">> Do Force Down");

		/* first try normal shutdown */
		pwrctrl_do_shutdown();

		ret = wait_sig_value(&slp_sig_PLTRST, -1, 0, 6000);
		if (!ret) {
			LOG_INF(">> Normal down failed, try Force Down");

			gpio_write_pin(PM_PWRBTN, 0);

			ret = wait_sig_value(&in_force_down, 1, 0, 7200);
			if (!ret) {
				LOG_INF(">> Wait Sig 1 FAIL");
				gpio_write_pin(PM_PWRBTN, 1);
			}
		}

		LOG_INF(">> Do Force Down end");
	}
	else {
		LOG_DBG(">> Skip Force Down");
	}
}

static void pwrctrl_do_hard_reset()
{
	LOG_INF(">> Do Hard Reset");

	gpio_write_pin(SOC_RSTBTN_N, 0);
	k_msleep(20);
	gpio_write_pin(SOC_RSTBTN_N, 1);

	LOG_INF(">> Do Hard Reset end");
}

static void pwrctrl_forcedown_post(void)
{
	if (in_force_down) {
		gpio_write_pin(PM_PWRBTN, 1);
		in_force_down = 0;

		/* The system is already in the process of powering down,
		   disable SCI */
		g_acpi_state_flags.sci_enabled = 0;
	}
}

static void pwrctrl_worker(struct k_work *work)
{
	struct pwrctrl_work_data *data = CONTAINER_OF(work, struct pwrctrl_work_data, work_item);

	switch (data->act)
	{
	case PWC_UP:
		pwrctrl_do_up();
		break;
	case PWC_SHUTDOWN: /* gracefull shutdown */
		pwrctrl_do_shutdown();
		break;
	case PWC_HARD_RESET:
		pwrctrl_do_hard_reset();
		break;
	case PWC_FORCE_DOWN: /* power off */
		pwrctrl_do_force_down();
		break;
	default:
		return;
	}
}

static int do_power_ctrl(uint8_t* req)
{
	pwrctrl_work_data.act = req[0];
	pwrctrl_work_data.sec = req[1];

	uint32_t status = k_work_busy_get(&pwrctrl_work_data.work_item);
	if (status & K_WORK_RUNNING) {
		LOG_WRN("PwrCtrl Work is still running");
		return -1;
	}

	k_work_submit(&pwrctrl_work_data.work_item);

	return 0;
}

static int lom_mgmt_handle_request(struct lom_mgmt_task *task)
{
	if (task->req == NULL || task->res == NULL) {
		LOG_ERR(">> No Request!");
		return -1;
	}

	LOG_DBG(">> Processing Func<%d>", task->req->func);

	uint8_t *req_data = task->req->data;
	uint8_t *res_data = task->res->data;
	uint8_t code = EC_RET_OK;
	uint8_t flag = 0;
	uint16_t dat_size = 0;
	uint8_t ret;
	int i;

	switch (task->req->func) {
	case FUNC_GET_BIOS_VER:
		break;
	case FUNC_POWER_CTRL:
		if (do_power_ctrl(req_data) < 0) {
			code = EC_RET_ERR_IN_PROCESS;
		}
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

		LOG_DBG("sensors data size %d", dat_size);

#if 0
		LOG_INF("+=+ 0x%02x 0x%02x 0x%02x 0x%02x",
				res_data[0], res_data[1], res_data[2], res_data[3]);
		k_sleep(K_MSEC(delays[rnd_idx++ % RND_CNT]));
#endif
		break;
	case FUNC_GET_FRU:
		ret = do_get_fru(res_data, &dat_size);
		if (ret < 0) {
			code = EC_RET_ERR_FAIL;
			dat_size = -ret; /* fail code */
			break;
		}

		break;
	case FUNC_GET_EVENTS:
		if (do_get_events(&res_data[4], &dat_size) < 0) {
			LOG_ERR("get events failed");
			code = EC_RET_ERR_INV_SIZE;
			break;
		}

		if (dat_size) {
			dat_size += 4; /* reserved 4bytes space for timestamp */
			flag = RES_META_F_TIMESTAMP;
		}

		AVAIL_RES_EVENT_SET(0);

		LOG_DBG("events size %d", dat_size);

		break;

	case FUNC_GET_POSTCODE:
#ifdef CONFIG_POSTCODE_MONITOR
		do_get_postcode(&res_data[4], &dat_size);
		if (dat_size) {
			dat_size += 4; /* reserved 4bytes space for timestamp */
			flag = RES_META_F_TIMESTAMP;
		}

		AVAIL_RES_POSTCODE_SET(0);

		LOG_DBG("postcode size %d", dat_size);

#else
		code = EC_RET_ERR_NOT_IMPL;
#endif

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

		/* reserved 4bytes space for timestamp */
		for (i = 4; i < dat_size; i++) {
			res_data[i] = i;
		}

		break;

	default:
		code = EC_RET_ERR_INV_FUNC;
		LOG_ERR("Unknown func %d, code %d", task->req->func, code);
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

#ifdef VWI_SIG_DBG
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

static uint16_t boot_cycle_count = 0;

/*
 * PLTRST, SLP_A, SLP_S5, SLP_S4, SLP_S3, SLP_WLAN = 1 : System Power UP
 * PLTRST, SLP_A, SLP_S5, SLP_S4, SLP_S3, SLP_WLAN = 0 : System Power Down
 * SUS_PWRDN_ACK = 0: Power UP; = 1: Power Down
 *
 * In DN process, the PLTRST = 0, is the first signal
 * In UP process, the PLTRST = 1, is the last signal
 *
 * SLA_A = 0, Starting DN process
 * SLA_A = 1, Starting UP process
 *
 */
static void espi_vwire_monitor(const struct device *dev, struct espi_callback *cb,
	struct espi_event event)
{
	if (event.evt_details > ESPI_VWIRE_SIGNAL_SUS_ACK) {
		LOG_DBG(">> VWI_EVT: SIG[%2d] %13s: %x",
			event.evt_details, "UNKNOWN", event.evt_data);
		return;
	}

	uint8_t pwr_sta = pwrseq_system_state();

#ifdef VWI_SIG_DBG
	LOG_INF(">> VWI_EVT: SIG[%2d] %13s: %x (PWRS:%d)", event.evt_details,
		vwi_managed_sigs[event.evt_details].name, event.evt_data, pwr_sta);
#endif

	switch (event.evt_details) {
	case ESPI_VWIRE_SIGNAL_SLP_WLAN:
		if (event.evt_data == 0) {
			pwrctrl_forcedown_post();
		}
		break;
	case ESPI_VWIRE_SIGNAL_SLP_S5:
		if (event.evt_data == 0) { /* power off sequence */
			if (pwr_sta == SYSTEM_S0_STATE) {
				LOG_INF(">> [HOST Soft Off]");
				host_event_put(HOST_EVENT_SOFT_DN);
				AVAIL_RES_EVENT_SET(1);
			}
		}
		break;
	case ESPI_VWIRE_SIGNAL_SLP_S3:
		if (event.evt_data == 1) { /* power on sequence */
			if (pwr_sta == SYSTEM_G3_STATE) {
				LOG_INF(">> [HOST Power Up]");
				host_event_put(HOST_EVENT_POWER_UP);
				AVAIL_RES_EVENT_SET(1);
				boot_cycle_count++;
			}
			else if (pwr_sta > SYSTEM_S0_STATE) {
				LOG_INF(">> [HOST Soft Up]");
				host_event_put(HOST_EVENT_SOFT_UP);
				AVAIL_RES_EVENT_SET(1);
				boot_cycle_count++;
			}
		}
		break;
	case ESPI_VWIRE_SIGNAL_PLTRST:
		slp_sig_PLTRST = event.evt_data;
		if (event.evt_data) {
			if (pwr_sta == SYSTEM_S0_STATE) {
				LOG_INF(">> [Host Reboot]");
				host_event_put(HOST_EVENT_REBOOT);
				boot_cycle_count++;
			}
		}
		break;
	case ESPI_VWIRE_SIGNAL_HOST_RST_WARN:
		if (event.evt_data == 1) {
			LOG_INF(">> [HOST RST]");
			host_event_put(HOST_EVENT_HOST_RESET);
			AVAIL_RES_EVENT_SET(1);
		}
		break;
	case ESPI_VWIRE_SIGNAL_DNX_WARN:
		if (event.evt_data == 1) {
			LOG_INF(">> [DNX WARN]");
			host_event_put(HOST_EVENT_DNX_WARN);
			AVAIL_RES_EVENT_SET(1);
		}
		break;
	default:
	}
}

#if 0
static void espi_bus_reset_monitor(const struct device *dev, struct espi_callback *cb,
	struct espi_event event)
{
	LOG_DBG("eSPI BUS reset %d", event.evt_data);
}
#endif

static struct espi_callback espi_vwi_cb;
static struct espi_callback espi_rst_cb;
static int event_monitor_inited = 0;

void init_espi_event_monitor(void)
{
	if (!device_is_ready(espi_dev)) {
		LOG_ERR("%s: device not ready.", espi_dev->name);
		return;
	}

	//espi_init_callback(&espi_rst_cb, espi_bus_reset_monitor, ESPI_BUS_RESET);
	espi_init_callback(&espi_vwi_cb, espi_vwire_monitor, ESPI_BUS_EVENT_VWIRE_RECEIVED);

	espi_add_callback(espi_dev, &espi_rst_cb);
	espi_add_callback(espi_dev, &espi_vwi_cb);

	event_monitor_inited = 1;
}

static void pwrbtn_event_handler(uint8_t pwrbtn_sts)
{
	if (pwrbtn_sts == 1) /* 1 is UP */
		return;

	LOG_INF(">> Power Button Pressed");
	host_event_put(HOST_EVENT_PWRBTN_DN);
}

#if 0
static void pwrbtn_event_handler(uint8_t pwrbtn_sts)
{
	if (pwrbtn_sts == 1) /* 1 is UP */
		return;

	uint8_t pwr_sta = pwrseq_system_state();

	if (pwr_sta != SYSTEM_G3_STATE && pwr_sta != SYSTEM_S5_STATE) {
		LOG_INF(">> Button Power Down (PWRS:%d)", pwr_sta);
		host_event_put(HOST_EVENT_PWRBTN_DN);
	}
	else {
		LOG_INF(">> Button Power UP (PWRS:%d)", pwr_sta);
		host_event_put(HOST_EVENT_PWRBTN_UP);
	}
}
#endif

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

#ifdef CONFIG_POSTCODE_MONITOR
static void postcode_disp_event_handler(uint16_t code)
{
	//LOG_INF("==[%d] PostCode 0x%04X", boot_cycle_count, code);

	if (postcode_add(code, boot_cycle_count) < 0) {
		return;
	}

	AVAIL_RES_POSTCODE_SET(1);
}

void init_postcode_disp_event_monitor(void)
{
	postcode_add_disp_event_handler(postcode_disp_event_handler);

	LOG_INF(">> PostCode Disp Event Handler Registered");
}
#endif

void lom_mgmt_thread(void *p1, void *p2, void *p3)
{
	uint32_t normal_period = *(uint32_t *)p1;

	init_espi_event_monitor();
	init_gpio_event_monitor();
#ifdef CONFIG_POSTCODE_MONITOR
	init_postcode_disp_event_monitor();
#endif

	wait_hwdata_ready(normal_period);

	k_work_init(&pwrctrl_work_data.work_item, pwrctrl_worker);

	lom_mgmt_i2c_set_avail_res(lom_mgmt_dev, 0xFF, 0);
	lom_mgmt_i2c_set_callbacks(lom_mgmt_dev, &callbacks);

	while (true) {
		k_sem_take(&lom_mgmt_sem, K_FOREVER);

		lom_mgmt_handle_request(&lom_mgmt_task);
	}
}
