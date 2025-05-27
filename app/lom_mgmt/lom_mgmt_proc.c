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

#include "pwrplane.h"
#include "pwrbtnmgmt.h"
#include "espi_hub.h"
#include "task_handler.h"
#include "rstbutton.h"

#include "host_event.h"
#include "lom_mgmt_i2c.h"

LOG_MODULE_REGISTER(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define CPU_TEMP_CS_ACCESS_PERIOD_SEC 8U

static const struct device *const espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

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

static inline void fill_one_sensor(struct sensor_record *srd, const struct hwmon_sram_entry_desc * ent)
{
	uint16_t *sram = (uint16_t *)hwmon_data;

	srd->info  = ent->sens_info;
	if (ent->kind != HWMON_KIND_FRPM) {
		srd->info |= (uint8_t)(sram[ent->offset/2 + 7] & 0x1) << SENS_INFO_MUL_BIT; /* multiplier */
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
		LOG_INF(">> acpi raw 0x%x", s);
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

	wait_hwdata_ready(normal_period);

	lom_mgmt_i2c_set_callbacks(lom_mgmt_dev, &callbacks);

	while (true) {
		k_sem_take(&lom_mgmt_sem, K_FOREVER);

		lom_mgmt_handle_request(&lom_mgmt_task);
	}
}
