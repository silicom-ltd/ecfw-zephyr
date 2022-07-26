/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2022 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include "mec172x_pin.h"
#include "common_mec172x.h"

#ifndef __AZBEACH_MEC172X_H__
#define __AZBEACH_MEC172X_H__

extern uint8_t platformskutype;

#define KSC_PLAT_NAME                   "AZB"

/* I2C addresses */
#define EEPROM_DRIVER_I2C_ADDR          0x50

/* Signal to gpio mapping for MEC172x based ADL-M is described here */

#define PM_SLP_SUS			EC_GPIO_000
#define RSMRST_PWRGD_G3SAF_P		EC_GPIO_012
#define RSMRST_PWRGD_MAF_P		EC_GPIO_227

#define RSMRST_PWRGD			((boot_mode_maf == 1) ? \
					 RSMRST_PWRGD_MAF_P : \
					 RSMRST_PWRGD_G3SAF_P)

#define G3_SAF_DETECT			EC_GPIO_013
#define CPU_C10_GATE			EC_GPIO_022
#define PS_ON_OUT			EC_GPIO_023
#define EC_PCH_SPI_OE_N			EC_GPIO_024
#define PM_BAT_STATUS_LED2		EC_GPIO_035
#define SYS_PWROK			EC_GPIO_043

#define PM_RSMRST_G3SAF_P		EC_GPIO_054
#define PM_RSMRST_MAF_P			EC_GPIO_055
#define PM_RSMRST			((boot_mode_maf == 1) ? \
					 PM_RSMRST_MAF_P : \
					 PM_RSMRST_G3SAF_P)

#define ALL_SYS_PWRGD			EC_GPIO_057
/* We poll this GPIO in MAF mode in order to sense the input signal040_076.
 * This pin was already configured in pinmux as ALT mode 1 NOT GPIO
 */
#define ESPI_RESET_MAF			EC_GPIO_061

#define PM_PWRBTN			EC_GPIO_101
#define EC_SMI				EC_GPIO_102
#define PCH_PWROK			EC_GPIO_106
#define WAKE_SCI			EC_GPIO_114
#define DNX_FORCE_RELOAD_EC		EC_GPIO_115
#define CATERR_LED_DRV			EC_GPIO_153
#define SX_EXIT_HOLDOFF_N		EC_GPIO_175
#define PWRBTN_EC_IN_N			EC_DUMMY_GPIO_HIGH
#define PM_DS3				EC_DUMMY_GPIO_HIGH
#define TIMEOUT_DISABLE			EC_DUMMY_GPIO_LOW
#define FAN_PWR_DISABLE_N		EC_DUMMY_GPIO_LOW
#define PM_BATLOW			EC_DUMMY_GPIO_HIGH
#define THERM_STRAP			EC_DUMMY_GPIO_LOW

#define PM_SLP_S0_CS			EC_GPIO_221
#define PM_SLP_S0_EC_N			EC_GPIO_240		/* ADL-M */
#define EC_PWRBTN_LED			EC_GPIO_014		/* FP Left Blue */

#define EC_PG3_EXIT			EC_DUMMY_GPIO_LOW
#define BC_ACOK				EC_DUMMY_GPIO_HIGH
#define VOL_UP				EC_DUMMY_GPIO_HIGH
#define VOL_DOWN			EC_DUMMY_GPIO_HIGH
#define HOME_BUTTON			EC_DUMMY_GPIO_HIGH
#define SMC_LID				EC_DUMMY_GPIO_HIGH
#define DG2_PRESENT			EC_DUMMY_GPIO_LOW
#define PEG_RTD3_COLD_MOD_SW_R		EC_DUMMY_GPIO_LOW
#define VIRTUAL_DOCK			EC_DUMMY_GPIO_LOW
#define VIRTUAL_BAT			EC_DUMMY_GPIO_LOW

/* XXX JJD, only an output and can't be a GPIO */
#define PROCHOT				EC_BGPO_000

/* Device instance names */
#define I2C_BUS_0			DT_LABEL(DT_NODELABEL(i2c_smb_0))
#define I2C_BUS_1			DT_LABEL(DT_NODELABEL(i2c_smb_1))
#define ESPI_0				DT_LABEL(DT_NODELABEL(espi0))
#define SPI_0				DT_LABEL(DT_NODELABEL(spi0))
#define ADC_CH_BASE			DT_LABEL(DT_NODELABEL(adc0))
#define PECI_0_INST			DT_LABEL(DT_NODELABEL(peci0))
#define WDT_0				DT_LABEL(DT_NODELABEL(wdog))

/* Button/Switch Initial positions */
#define PWR_BTN_INIT_POS		1
#define VOL_UP_INIT_POS			1
#define VOL_DN_INIT_POS			1
#define LID_INIT_POS			1
#define HOME_INIT_POS			1
#define VIRTUAL_DOCK_INIT_POS		1
#define VIRTUAL_BAT_INIT_POS		1

/* Minimum Adapter power(Milli Watts) for proceeding with boot */
#define ADP_CRIT_POWERUP		26000

#define TIPD_PORT_0_I2C_ADDR	0x20U
#define TIPD_PORT_1_I2C_ADDR	0x24U
#define TIPD_PORT_2_I2C_ADDR	0x21U
#define TIPD_PORT_3_I2C_ADDR	0x25U

/* PD version: MSB byte represent Major version and
 * LSB byte represent Minor version
 */
#define USB_PD_VERSION 0x0200

#endif /* __AZBEACH_MEC172X_H__ */
