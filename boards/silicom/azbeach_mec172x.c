/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <drivers/gpio.h>
#include "i2c_hub.h"
#include <logging/log.h>
#include "gpio_ec.h"
#include "espi_hub.h"
#include "board.h"
#include "board_config.h"
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
	{ PM_SLP_SUS,		GPIO_INPUT },
	{ EC_GPIO_011,		GPIO_INPUT },
	{ RSMRST_PWRGD_G3SAF_P,	GPIO_INPUT },
	{ RSMRST_PWRGD_MAF_P,	GPIO_INPUT },
	{ EC_GPIO_015,		GPIO_INPUT },
	{ EC_GPIO_023,		GPIO_INPUT },
	{ EC_GPIO_025,		GPIO_INPUT },
	{ SMC_LID,		GPIO_INPUT },
	{ EC_GPIO_035,		GPIO_INPUT },
	{ EC_GPIO_036,		GPIO_INPUT },
	{ SYS_PWROK,		GPIO_OUTPUT_LOW | GPIO_OPEN_DRAIN },
	{ EC_GPIO_050,		GPIO_INPUT },
	{ EC_GPIO_052,		GPIO_INPUT },
	{ ALL_SYS_PWRGD,	GPIO_INPUT },
	{ FAN_PWR_DISABLE_N,	GPIO_OUTPUT_HIGH },
	{ EC_GPIO_067,		GPIO_INPUT },
	{ EC_GPIO_100,		GPIO_INPUT },
	{ EC_GPIO_104,		GPIO_INPUT },
	{ PCH_PWROK,		GPIO_OUTPUT_LOW },
	{ WAKE_SCI,		GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN },
#ifdef CONFIG_DNX_EC_ASSISTED_TRIGGER
	{ DNX_FORCE_RELOAD_EC,	GPIO_OUTPUT_LOW},
#else
	{ DNX_FORCE_RELOAD_EC,	GPIO_INPUT},
#endif
	/* PM_BATLOW NA for S platfroms, so make it input */
	{ PM_BATLOW,		GPIO_INPUT },
	{ CATERR_LED_DRV,	GPIO_INPUT },
	{ EC_GPIO_161,		GPIO_INPUT },
	{ PWRBTN_EC_IN_N,	GPIO_INPUT | GPIO_INT_EDGE_BOTH },
	{ BC_ACOK,		GPIO_INPUT },
	{ PS_ON_OUT,		GPIO_INPUT },
	{ CPU_C10_GATE,		GPIO_INPUT },
	{ EC_SMI,		GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN },
	{ PM_SLP_S0_CS,		GPIO_INPUT },
	{ PM_DS3,		GPIO_INPUT },
	{ SX_EXIT_HOLDOFF_N,	GPIO_INPUT },
	{ PM_PWRBTN,		GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN },
	{ DG2_PRESENT,		GPIO_INPUT },
	{ PEG_RTD3_COLD_MOD_SW_R, GPIO_INPUT },
	{ PROCHOT,		GPIO_OUTPUT_HIGH },
};

struct gpio_ec_config mecc172x_cfg_sus[] =  {
};

struct gpio_ec_config mecc172x_cfg_res[] =  {
};

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

int board_init(void)
{
	int ret;

	ret = gpio_init();
	if (ret) {
		LOG_ERR("Failed to initialize gpio devs: %d", ret);
		return ret;
	}

	ret = i2c_hub_config(I2C_0);
	if (ret) {
		return ret;
	}

	detect_boot_mode();

	ret = gpio_configure_array(mecc172x_cfg, ARRAY_SIZE(mecc172x_cfg));
	if (ret) {
		LOG_ERR("%s: %d", __func__, ret);
		return ret;
	}

	/* In MAF, boot ROM already made this pin output and high, so we must
	 * keep it like that during the boot phase in order to avoid espi reset
	 */
	if (espihub_boot_mode() == FLASH_BOOT_MODE_MAF) {
		/* Ensure GPIO mode for pins reconfigure due to QMSPI device */
		gpio_force_configure_pin(RSMRST_PWRGD_MAF_P,
					 GPIO_INPUT | GPIO_PULL_UP);
		gpio_force_configure_pin(PM_RSMRST_MAF_P, GPIO_OUTPUT_HIGH);

		/* LPM optimizations */
		gpio_configure_pin(PM_RSMRST_G3SAF_P, GPIO_DISCONNECTED);
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
