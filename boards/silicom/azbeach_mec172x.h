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

/* power/boot related */
#define PM_SLP_SUS			EC_GPIO_000
#define RSMRST_PWRGD_G3SAF_P		EC_GPIO_012
#define G3_SAF_DETECT			EC_GPIO_013
#define SOC_RSTBTN_N			EC_GPIO_020
#define CPU_C10_GATE			EC_GPIO_022
#define PS_ON_OUT			EC_GPIO_023
#define SYS_PWROK			EC_GPIO_043
#define PVT_SPI_BOOT			EC_GPIO_045
#define PLTRST_N			EC_GPIO_052
#define PM_RSMRST_G3SAF_P 		EC_GPIO_253
#define ALL_SYS_PWRGD			EC_GPIO_057
/* We poll this GPIO in MAF mode in order to sense the input signal040_076.
 * This pin was already configured in pinmux as ALT mode 1 NOT GPIO
 */
#define ESPI_RESET_MAF			EC_GPIO_061
#define PM_PWRBTN			EC_GPIO_101
#define PCH_PWROK			EC_GPIO_106
#define PM_BATLOW			EC_GPIO_140
#define SLP_S3_N			EC_GPIO_171
#define SLP_S4_N			EC_GPIO_172
#define SX_EXIT_HOLDOFF_N		EC_GPIO_175
#define RSMRST_PWRGD			EC_GPIO_227
#define PM_RSMRST			EC_GPIO_055

#define EC_PCH_SPI_OE_N			EC_GPIO_024

/* button/peripherals */
#define BTN_PROTRUDING			EC_GPIO_135
#define MIPI60_PWRBTN_N			EC_GPIO_162
#define BTN_RECESSED			EC_GPIO_166 
#define PWRBTN_EC_IN_N			BTN_PROTRUDING

#define EC_SMI				EC_GPIO_102
#define WAKE_SCI			EC_GPIO_114
#define DNX_FORCE_RELOAD_EC		EC_GPIO_115
#define CATERR_LED_DRV			EC_GPIO_153
#define PM_DS3				EC_DUMMY_GPIO_HIGH
#define TIMEOUT_DISABLE			EC_DUMMY_GPIO_LOW
#define FAN_PWR_DISABLE_N		EC_DUMMY_GPIO_LOW
#define THERM_STRAP			EC_DUMMY_GPIO_HIGH

/* (unused/DNI) */
#define PM_SLP_S0_CS			EC_GPIO_221
#define PM_SLP_S0_EC_N			EC_GPIO_240		/* ADL-M */
#define EC_PWRBTN_LED			EC_DUMMY_GPIO_LOW

/* M.2 Slots */
#define W_DISABLE_M2_SLOT1_N		EC_GPIO_026
#define W_DISABLE_M2_SLOT2_N		EC_GPIO_027
#define SIM_M2_SLOT1_MUX_SEL		EC_GPIO_030
#define SIM_M2_SLOT2_MUX_SEL		EC_GPIO_031
#define SLOT0_LED1_OUT			EC_GPIO_130
#define SLOT0_LED2_OUT			EC_GPIO_131
#define SIM_M2_SLOT2A_DET_N		EC_GPIO_152
#define SIM_M2_SLOT1A_DET_N		EC_GPIO_154
#define SIM_M2_SLOT1B_DET_N		EC_GPIO_155
#define SLOT3_SSD_PWRDIS		EC_GPIO_220
#define RST_CTL_M2_SLOT1_N		EC_GPIO_230
#define RST_CTL_M2_SLOT2_N		EC_GPIO_231
#define SIM_M2_SLOT2B_DET_N		EC_GPIO_232
#define EC_M_2_SSD_PLN			EC_GPIO_236
#define SLOT1_LED_OUT			EC_GPIO_242
#define SLOT2_LED_OUT			EC_GPIO_243

#define BC_ACOK				EC_DUMMY_GPIO_HIGH
#define DG2_PRESENT			EC_DUMMY_GPIO_LOW
#define PEG_RTD3_COLD_MOD_SW_R		EC_DUMMY_GPIO_LOW

#define PROCHOT				EC_GPIO_160

/* Device instance names */
#define I2C_BUS_0			DT_NODELABEL(i2c_smb_0)
#define I2C_BUS_1			DT_NODELABEL(i2c_smb_1)
#define ESPI_0				DT_NODELABEL(espi0)
#define SPI_0				DT_NODELABEL(spi0)
#define ADC_CH_BASE			DT_NODELABEL(adc0)
#define PECI_0_INST			DT_NODELABEL(peci0)
#define WDT_0				DT_NODELABEL(wdog)

/* Button/Switch Initial positions */
#define PWR_BTN_INIT_POS		1
#define BTN_RECESSED_INIT_POS		1

/* PD version: MSB byte represent Major version and
 * LSB byte represent Minor version
 */
#define USB_PD_VERSION 0x0200

#endif /* __AZBEACH_MEC172X_H__ */
