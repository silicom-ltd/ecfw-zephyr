/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <zephyr/device.h>
#include <errno.h>
#include <zephyr/drivers/led.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/sys/util.h>
#include "i2c_hub.h"
#include <zephyr/logging/log.h>
#include "gpio_ec.h"
#include "led_mec172x.h"
#include "espi_hub.h"
#include "board.h"
#include "board_config.h"
#include "vci_mec172x.h"
#include "azbeach_mec172x.h"

LOG_MODULE_DECLARE(board, CONFIG_BOARD_LOG_LEVEL);
/** @brief EC FW app owned gpios list.
 *
 * This list is not exhaustive, it do not include driver-owned pins,
 * the initialization is done as part of corresponding Zephyr pinmux driver.
 * BSP drivers are responsible to control gpios in soc power transitions and
 * system transitions.
 *
 * Note: Pins not assigned to any app function are identified with their
 * original pin number instead of signal
 *
 */

/* APP-owned gpios */
struct gpio_ec_config mecc172x_cfg[] = {
	{ PLTRST_N,		GPIO_INPUT },
	{ PM_SLP_SUS,		GPIO_INPUT },
	{ EC_GPIO_011,		GPIO_INPUT },
	{ RSMRST_PWRGD_G3SAF_P,	GPIO_INPUT },
	{ RSMRST_PWRGD,		GPIO_INPUT },
	{ EC_GPIO_015,		GPIO_INPUT },	/* W_DISABLE_M2_SLOT3_N */
	{ SYS_PWROK,		GPIO_OUTPUT_LOW },
	{ ALL_SYS_PWRGD,	GPIO_INPUT },
	{ FAN_PWR_DISABLE_N,	GPIO_OUTPUT_HIGH },
	{ PCH_PWROK,		GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN },
	{ WAKE_SCI,		GPIO_OUTPUT_HIGH },
#ifdef CONFIG_DNX_EC_ASSISTED_TRIGGER
	{ DNX_FORCE_RELOAD_EC,	GPIO_OUTPUT_LOW},
#else
	{ DNX_FORCE_RELOAD_EC,	GPIO_INPUT},
#endif
	{ PM_BATLOW,		GPIO_OUTPUT_HIGH },
	{ CATERR_LED_DRV,	GPIO_INPUT },
	{ PWRBTN_EC_IN_N,	GPIO_INPUT | GPIO_INT_EDGE_BOTH },
	{ BC_ACOK,		GPIO_INPUT },
	{ PS_ON_OUT,		GPIO_OUTPUT_LOW },
	{ CPU_C10_GATE,		GPIO_INPUT },
	{ EC_SMI,		GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN },
	{ PM_SLP_S0_CS,		GPIO_INPUT },
	{ PM_DS3,		GPIO_INPUT },
	{ SX_EXIT_HOLDOFF_N,	GPIO_INPUT },
	{ PM_PWRBTN,		GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN },
	{ DG2_PRESENT,		GPIO_INPUT },
	{ PEG_RTD3_COLD_MOD_SW_R, GPIO_INPUT },
	{ BTN_RECESSED,		GPIO_INPUT },
	{ SOC_RSTBTN_N,		GPIO_OUTPUT_HIGH },
	{ SLP_S3_N,             GPIO_INPUT },
	{ SLP_S4_N,             GPIO_INPUT },
	{ SIM_M2_SLOT1A_DET_N,	GPIO_INPUT },
	{ SIM_M2_SLOT1B_DET_N,	GPIO_INPUT },
	{ SIM_M2_SLOT2A_DET_N,	GPIO_INPUT },
	{ SIM_M2_SLOT2B_DET_N,	GPIO_INPUT },
	{ SLOT3_SSD_PWRDIS,	GPIO_OUTPUT_LOW },
	{ EC_M_2_SSD_PLN,	GPIO_OUTPUT_HIGH },
};

struct gpio_ec_config mecc172x_vci_cfg[] = {
	{ MIPI60_PWRBTN_N, 	GPIO_INPUT },
};

struct gpio_ec_config mecc172x_cfg_sus[] =  {
};

struct gpio_ec_config mecc172x_cfg_res[] =  {
};

#ifdef CONFIG_LED_MANAGEMENT
#define DT_PWM_MC_LED_INST(x)	DT_NODELABEL(pwmmcled##x)
#define DT_PWM_LED_INST(x)	DT_NODELABEL(pwmled##x)
#define DT_GPIO_LED_INST(x)	DT_NODELABEL(gpioled##x)

static struct led_dev led_tbl[32];
static uint8_t max_led_dev;

static void init_led_devices(void)
{
	int i = 0;

#if DT_NODE_HAS_STATUS(DT_PWM_MC_LED_INST(0), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_MC_LED_INST(0));
	led_tbl[i].rgb = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_MC_LED_INST(1), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_MC_LED_INST(1));
	led_tbl[i].rgb = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_MC_LED_INST(2), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_MC_LED_INST(2));
	led_tbl[i].rgb = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_MC_LED_INST(3), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_MC_LED_INST(3));
	led_tbl[i].rgb = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(0), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(0));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(1), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(1));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(2), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(2));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(3), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(3));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(4), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(4));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(5), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(5));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(6), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(6));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(7), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(7));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(8), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(8));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(9), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(9));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(10), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(10));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(11), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_PWM_LED_INST(11));
	led_tbl[i].pwm = 1;
	led_off(led_tbl[i].dev, 0); 
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(0), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(0));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(1), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(1));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(2), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(2));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(3), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(3));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(4), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(4));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(5), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(5));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
#if DT_NODE_HAS_STATUS(DT_GPIO_LED_INST(6), okay)
	led_tbl[i].dev = DEVICE_DT_GET(DT_GPIO_LED_INST(6));
	led_off(led_tbl[i].dev, 0);
	i++;
#endif
	max_led_dev = i;
}

void board_led_dev_tbl_init(uint8_t *max, const struct led_dev **tbl)
{
	init_led_devices();

	*max = max_led_dev;
	*tbl = led_tbl;
}

#endif

#ifdef CONFIG_THERMAL_MANAGEMENT
/**
 * @brief Fan device table.
 *
 * This table lists the supported fan devices for board. By default, each
 * board is assigned one fan for CPU.
 */
static struct fan_dev fan_tbl[] = {
/*	PWM_CH_##	TACH_CH_##  */
	{ PWM_CH_00,	TACH_CH_00 }, /* Fan 1 */
	{ PWM_CH_01,	TACH_CH_01 }, /* Fan 2 */
};


/**
 * @brief Thermal sensor table.
 *
 * This table lists the thermal sensors connected to the board and their
 * respective ACPI location field it is mapped to update the temperature value.
 */

static struct therm_sensor therm_sensor_tbl_azbeach[] = {
/*      ADC_CH_##	ACPI_LOC		dtt_threshold */
	{ADC_CH_04,	ACPI_THRM_SEN_AMBIENT,	{0} },  /* ADC_AMB */
	{ADC_CH_05,	ACPI_THRM_SEN_VR,	{0} },	/* ADC_VR */
	{ADC_CH_06,	ACPI_THRM_SEN_DDR,	{0} },	/* ADC_DDR*/
	{ADC_CH_10,	ACPI_THRM_SEN_EXTCPU,	{0} },  /* ADC_CPU */
	/* there is also a CPU temp sensor */
};

void board_therm_sensor_tbl_init(uint8_t *p_max_adc_sensors,
		struct therm_sensor **p_therm_sensor_tbl)
{
	*p_therm_sensor_tbl = therm_sensor_tbl_azbeach;
	*p_max_adc_sensors = ARRAY_SIZE(therm_sensor_tbl_azbeach);
}

void board_fan_dev_tbl_init(uint8_t *pmax_fan, struct fan_dev **pfan_tbl)
{
	*pfan_tbl = fan_tbl;
	*pmax_fan = ARRAY_SIZE(fan_tbl);
}

#endif
static int wait_for_pin_level(uint32_t port_pin, uint16_t timeout,
			uint32_t exp_level)
{
	uint16_t loop_cnt = timeout;
	int level;

	do {
		/* Passes the enconded gpio(port_pin) to the gpio driver */
		level = gpio_read_pin(port_pin);
		if (level < 0) {
			LOG_ERR("Failed to read %x ", gpio_get_pin(port_pin));
			return -EIO;
		}

		if (exp_level == level) {
			LOG_DBG("Pin [%d]: %x",
				get_absolute_gpio_num(port_pin), exp_level);
			break;
		}

		k_sleep(K_MSEC(100));
		loop_cnt--;
	} while (loop_cnt > 0);

	if (loop_cnt == 0) {
		LOG_DBG("Timeout [%x]: %x", gpio_get_pin(port_pin), level);
		return -ETIMEDOUT;
	}

	return 0;
}

static inline int wait_for_pin(uint32_t port_pin, uint16_t timeout,
			       uint32_t exp_level)
{
	return wait_for_pin_level(port_pin, timeout, exp_level);
}

#if 1

const uint8_t gamma8[] = {
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
	  1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
	  2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,
	  5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,
	 10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,
	 17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
	 25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,
	 37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,
	 51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68,
	 69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,
	 90,  92,  93,  95,  96,  98,  99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
	115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
	144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
	177, 180, 182, 184, 184, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
	215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

#ifdef CONFIG_BOARD_POST 
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT gpio_leds
void run_led_tests(void)
{
#define LED_GPIO(inst) DEVICE_DT_GET(DT_NODELABEL(gpioled##inst)),
	const struct device *leds[] = {
		DT_INST_FOREACH_STATUS_OKAY(LED_GPIO)
	};
	const int num_leds = ARRAY_SIZE(leds);
	const struct device *led_gpio;
	int i,err;

	for (i = 0; i < num_leds; i++) {
		led_gpio = leds[i];
		err = led_on(led_gpio, 0);
		if (err < 0) {
			LOG_ERR("err=%d",err);
			return;
		}
		k_sleep(K_MSEC(1000));

		err = led_off(led_gpio, 0);
		k_sleep(K_MSEC(250));
	}
}

#if DT_NODE_HAS_STATUS(DT_PWM_MC_LED_INST(0), okay)
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT pwm_leds_multicolor
#define PWM_LED_MC(n) DEVICE_DT_GET(n),
static int color_table[] = {
        0xFF0000, /* magenta-ish */
        0x00FF00, /* dull green */
        0x0000FF, /* a nice blue */
};
void run_pwm_mc_led_tests(void)
{
	const struct device *mc_leds[] = {
		DT_INST_FOREACH_CHILD(0, PWM_LED_MC)
	};
	const int num_leds = ARRAY_SIZE(mc_leds);
	const struct device *led;
	int color_choice;
	uint8_t colors[3];
	uint8_t num_colors;
	uint16_t level;
	int i, err;

	for (i = 0; i < num_leds; i++) {
		led = mc_leds[i];

		err = led_on(led, 0);
		if (err < 0) {
			LOG_ERR("mc led err=%d", err);
			return;
		}
		k_sleep(K_MSEC(1000));

		err = led_off(led, 0);
		k_sleep(K_MSEC(250));
		color_choice = i;
		num_colors = ARRAY_SIZE(colors);
		while (num_colors--) {
			colors[0] = color_table[color_choice] >> 16;
			colors[1] = (color_table[color_choice] >> 8) & 0xff;
			colors[2] = color_table[color_choice] & 0xff;
			led_set_color(led, 0, 3, colors);
			for (level = 0; level < 255; level++) {
				led_set_brightness(led, 0, level);
				k_sleep(K_MSEC(5));
			}
			for (level = 255; level >= 0; level--) {
				if (level == 0) {
					led_off(led, 0);
					break;
				}
				led_set_brightness(led, 0, level);
				k_sleep(K_MSEC(5));
			}
			color_choice++;
			if (color_choice == 3)
				color_choice = 0;
		}
	}
}

#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(0), okay)
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT pwm_leds
#define LED_PWM_NODE_ID DT_COMPAT_GET_ANY_STATUS_OKAY(pwm_leds)
void run_pwm_led_tests(void)
{
	const char *led_label[] = {
        	DT_FOREACH_CHILD_SEP_VARGS(LED_PWM_NODE_ID, DT_PROP_OR, (,), label, NULL)
	};

	const int num_pwm_leds = ARRAY_SIZE(led_label);
	const struct device *led_pwm;
	int i,err;
	uint16_t duty_cycle;

	led_pwm = DEVICE_DT_GET(LED_PWM_NODE_ID);
	for (i = 0; i < num_pwm_leds; i++) {
		LOG_INF("Turning off led %d - %s", i, (led_label[i] ? : "no label"));
		led_off(led_pwm, i);
	}
	
	for (i = 0; i < num_pwm_leds; i++) {
		LOG_INF("Testing PWM LED %d - %s", i, (led_label[i] ? : "no label"));

		for (duty_cycle = 100; duty_cycle >0; duty_cycle--) {
			err = led_set_brightness(led_pwm, i, duty_cycle);
			LOG_INF("duty_cycle %d", duty_cycle);
			k_sleep(K_MSEC(10));
		}

		for (duty_cycle = 0; duty_cycle < 100; duty_cycle++) {
			err = led_set_brightness(led_pwm, i, duty_cycle);
			LOG_INF("duty_cycle %d", duty_cycle);
			k_sleep(K_MSEC(10));
		}
		led_off(led_pwm, i);
	}
}
#endif
#endif

#ifdef CONFIG_THERMAL_MANAGEMENT
#define DT_FAN_INST(x)		DT_NODELABEL(pwm##x)
#define FAN_LABEL(x)		DT_LABEL(DT_FAN_INST(x))
#define DT_TACH_INST(x)		DT_NODELABEL(tach##x)
#define TACH_LABEL(x)		DT_LABEL(DT_TACH_INST(x))

#define FAN_PWM_MULT 8

void run_fan_tests(const struct device *fan_pwm, const char *fan_label,
		   const struct device *tach, const char *tach_label)
{
	struct sensor_value val;
	int err,ret,i,seconds;
	int duty_cycles[]  = {100, 50, 0};

	for (i = 0; i < ARRAY_SIZE(duty_cycles); i++) {
		LOG_INF("Duty Cycle = %d",duty_cycles[i]);
		err = pwm_set_cycles(fan_pwm, 0, FAN_PWM_MULT * 100,
				FAN_PWM_MULT * duty_cycles[i], 0);
		if (err) {
			LOG_ERR("Unable to set fan speed");
			return;
		}
		if (duty_cycles[i] == 100)
			seconds = 7;
		else
			seconds = 5;

		while (seconds--) {
			k_sleep(K_MSEC(1000));

			ret = sensor_sample_fetch_chan(tach, SENSOR_CHAN_RPM);
			if (ret) {
				LOG_ERR("Unable to read fan RPM");
				return;
			}
			ret = sensor_channel_get(tach, SENSOR_CHAN_RPM, &val);
			LOG_INF("Fan sampled %d RPM ", val.val1);
		}
	}

	
}

#ifdef CONFIG_SIM_DETECT_SLOT_POST
void sim_slot_detect_test()
{
	int err, pins;
	const struct device *led_gpio;
	uint32_t pin_detect[] = {SIM_M2_SLOT1A_DET_N, SIM_M2_SLOT1B_DET_N,
				 SIM_M2_SLOT2A_DET_N, SIM_M2_SLOT2B_DET_N };

	char *pin_names[] = { "SIM M2 SLOT1A (bottom left)", "SIM M2 SLOT1B (top left)",
			      "SIM M2 SLOT2A (bottom right)", "SIM M2 SLOT2B (top right)" };

	led_gpio = device_get_binding(LED_GPIO_DEV_NAME);

	LOG_INF("Testing SIM SlotX detection");

	for (pins = 0; pins < ARRAY_SIZE(pin_detect); pins++) { 
		LOG_INF("Pin %s", pin_names[pins]);
		err = wait_for_pin(pin_detect[pins], 200, 1);

		if (!err) {
			LOG_INF("Detected");
			err = led_on(led_gpio, 0);
			k_sleep(K_MSEC(500));
		} else {
			LOG_ERR("Timed out");
		}
		err = led_off(led_gpio, 0);
	}
}
#endif

void board_post(void)
{
#ifdef CONFIG_THERMAL_MANAGEMENT
	const struct device *fan_pwm, *tach;
	const char *fan_label, *tach_label; 
#endif
#if 0
#define DT_SPEC_AND_COMMA(node_id, prop, idx) \
        ADC_DT_SPEC_GET_BY_IDX(node_id, idx),

	/* Data of ADC io-channels specified in devicetree. */
	static const struct adc_dt_spec adc_channels[] = {
        DT_FOREACH_PROP_ELEM(DT_PATH(zephyr_user), io_channels,
                             DT_SPEC_AND_COMMA)
	};
        uint16_t buf;
        struct adc_sequence sequence = {
                .buffer = &buf,
                /* buffer size in bytes, not number of samples */
                .buffer_size = sizeof(buf),
        };
	int err,i;
#else
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT ntc_thermistor
#define THERM_GET(inst) \
	DEVICE_DT_GET(DT_DRV_INST(inst))

	int rc;
	struct sensor_value temp;
	const struct device *thermistor4 = DEVICE_DT_GET(DT_NODELABEL(therm4));

	rc = sensor_sample_fetch(thermistor4);
	sensor_channel_get(thermistor4, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	LOG_INF("Sensor thermistor read: %dC", temp.val1);

	const struct device *thermistor5 = DEVICE_DT_GET(DT_NODELABEL(therm5));
	rc = sensor_sample_fetch(thermistor5);
	sensor_channel_get(thermistor5, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	LOG_INF("Sensor thermistor read: %dC", temp.val1);

	const struct device *thermistor6 = DEVICE_DT_GET(DT_NODELABEL(therm6));
	rc = sensor_sample_fetch(thermistor6);
	sensor_channel_get(thermistor6, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	LOG_INF("Sensor thermistor read: %dC", temp.val1);

	const struct device *thermistor10 = DEVICE_DT_GET(DT_NODELABEL(therm10));
	rc = sensor_sample_fetch(thermistor10);
	sensor_channel_get(thermistor10, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	LOG_INF("Sensor thermistor read: %dC", temp.val1);

#endif

	LOG_ERR("Heading to run_led_tests");
	run_led_tests();
	LOG_ERR("Returned from run_led_tests");
	run_pwm_mc_led_tests();
#if DT_NODE_HAS_STATUS(DT_PWM_LED_INST(0), okay)
	run_pwm_led_tests();
#endif

#ifdef CONFIG_THERMAL_MANAGEMENT
#if DT_NODE_HAS_STATUS(DT_FAN_INST(0), okay)
	fan_pwm = DEVICE_DT_GET(DT_FAN_INST(0));
	fan_label = DT_PROP_OR(DT_FAN_INST(0), label, NULL);

#if DT_NODE_HAS_STATUS(DT_TACH_INST(0), okay)
	tach = DEVICE_DT_GET(DT_TACH_INST(0));
	tach_label = DT_PROP_OR(DT_TACH_INST(0), label, NULL);
#else
#error "Need tach0 associated with fan pwm0!"
#endif
	run_fan_tests(fan_pwm, fan_label, tach, tach_label);
#endif

#if DT_NODE_HAS_STATUS(DT_FAN_INST(1), okay)
	fan_pwm = DEVICE_DT_GET(DT_FAN_INST(1));
	fan_label = DT_PROP_OR(DT_FAN_INST(1), label, NULL);
#if DT_NODE_HAS_STATUS(DT_TACH_INST(1), okay)
	tach = DEVICE_DT_GET(DT_TACH_INST(1));
	tach_label = DT_PROP_OR(DT_TACH_INST(1), label, NULL);
#else
#error "Need tach1 assocated with fan pwm1!"
#endif
	run_fan_tests(fan_pwm, fan_label, tach, tach_label);
#endif
#endif
#if 0
        /* Configure channels individually prior to sampling. */
	for (i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		if (!device_is_ready(adc_channels[i].dev)) {
			LOG_INF("ADC controller device not ready");
			return;
		}

		err = adc_channel_setup_dt(&adc_channels[i]);
		if (err < 0) {
			LOG_INF("Could not setup channel #%d (%d)", i, err);
			return;
		}
	}

	k_sleep(K_MSEC(1000));

	for (i = 0U; i < ARRAY_SIZE(adc_channels); i++) {
		LOG_INF("- %s, channel %d: ",
			adc_channels[i].dev->name,
			adc_channels[i].channel_id);

		(void)adc_sequence_init_dt(&adc_channels[i], &sequence);

		err = adc_read(adc_channels[i].dev, &sequence);
		if (err < 0) {
			LOG_INF("Could not read (%d)\n", err);
			continue;
		}

		LOG_INF("ADC Ch %d : raw value %d", i, buf);
	}
#endif
#ifdef CONFIG_SIM_DETECT_SLOT_POST
	sim_slot_detect_test();
#endif
}
#endif

#endif

#endif
int board_init(void)
{
	struct wktmr_regs *weektmr = (struct wktmr_regs *)0x4000ac80;
	int ret;
	const struct device *eeprom = DEVICE_DT_GET(DT_ALIAS(eeprom0));
	const struct device *fru = DEVICE_DT_GET(DT_NODELABEL(fru));
#if 0
	const struct device *spd = DEVICE_DT_GET(DT_NODELABEL(spd));
#endif
	const char eeprom_data[] = { 0xAA, 0x55 };

	char read_data[256];

	weektmr->BGPO_PWR &= ~0x7U;

	ret = gpio_init();
	if (ret) {
		LOG_ERR("Failed to initialize gpio devs: %d", ret);
		return ret;
	}

	ret = gpio_configure_array(mecc172x_cfg, ARRAY_SIZE(mecc172x_cfg));
	if (ret) {
		LOG_ERR("%s: %d", __func__, ret);
		return ret;
	}

	detect_boot_mode();

#if 0
	ret = vci_init();
	if (ret) {
		LOG_ERR("Failed to initialize vci devs: %d", ret);
		return ret;
	}
#endif
	ret = gpio_configure_array(mecc172x_vci_cfg, ARRAY_SIZE(mecc172x_vci_cfg));
	if (ret) {
		LOG_ERR("%s: %d", __func__, ret);
		return ret;
	}

#ifdef CONFIG_BOARD_POST 
	board_post();
#endif
	ret = eeprom_read(eeprom, 0, read_data, 16);
	if (ret < 0 || (read_data[0] == 0xFF)) {
		ret = eeprom_write(eeprom, 0, eeprom_data, 2);
		if (ret < 0) {
			LOG_ERR("%s: %d, Unable to write eeprom", __func__, ret);
		} else {
			LOG_INF("%s: Wrote 2 bytes to eeprom", __func__);
		}
	} else {
		LOG_INF("%s: Read 16 bytes from eeprom, 0x%x 0x%x 0x%x 0x%x 0x%x 0x0%x 0x%x 0x%x\n\
			0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x....", __func__, read_data[0], read_data[1],
			read_data[2], read_data[3], read_data[4], read_data[5], read_data[6], read_data[7], read_data[8],
			read_data[9], read_data[10], read_data[11], read_data[12], read_data[13], read_data[14], read_data[15]);
	}
#if 1
	ret = eeprom_read(fru, 0, read_data, 8);
	if (ret < 0) {
		LOG_ERR("%s: %d, Unable to read eeprom", __func__, ret);
	} else {
		LOG_INF("%s: Read 8 bytes from spd, 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x...", __func__, read_data[0], read_data[1], read_data[2], read_data[3], read_data[4], read_data[5], read_data[6], read_data[7]);
	}

	/* XXX Valencia, after reading FRU and doing stuff, set the SMBus pins to inputs to disable */
	gpio_force_configure_pin(EC_GPIO_007, GPIO_INPUT);
	gpio_force_configure_pin(EC_GPIO_010, GPIO_INPUT);

#endif
	/* In MAF, boot ROM already made this pin output and high, so we must
	 * keep it like that during the boot phase in order to avoid espi reset
	 */
	if (espihub_boot_mode() == FLASH_BOOT_MODE_MAF) {
		/* Ensure GPIO mode for pins reconfigure due to QMSPI device */
		gpio_force_configure_pin(RSMRST_PWRGD,
					 GPIO_INPUT | GPIO_PULL_UP);
		gpio_force_configure_pin(PM_RSMRST, GPIO_OUTPUT_HIGH);

		/* LPM optimizations */
		gpio_configure_pin(PM_RSMRST_G3SAF_P, GPIO_DISCONNECTED);
	} else if (gpio_read_pin(PVT_SPI_BOOT) == 1) {
		/* ensure no ESPI operations happen when in VTT testing mode */
		espihub_set_boot_mode(FLASH_BOOT_MODE_OWN);

		gpio_force_configure_pin(RSMRST_PWRGD, GPIO_INPUT | GPIO_PULL_UP);
	} else {
		gpio_configure_pin(PM_RSMRST_G3SAF_P, GPIO_OUTPUT_LOW);
	}

	return 0;
}

int board_suspend(void)
{
	int ret;

	ret = gpio_configure_array(mecc172x_cfg_sus,
				   ARRAY_SIZE(mecc172x_cfg_sus));
	if (ret) {
		LOG_ERR("%s: %d", __func__, ret);
		return ret;
	}


	return 0;
}

int board_resume(void)
{
	int ret;

	ret = gpio_configure_array(mecc172x_cfg_res,
				   ARRAY_SIZE(mecc172x_cfg_res));
	if (ret) {
		LOG_ERR("%s: %d", __func__, ret);
		return ret;
	}

	return 0;
}
