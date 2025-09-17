/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024-2025 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include "mec172x_pin.h"
#include "common_mec172x.h"

#ifndef __ADL_N_CADIZ_MEC172X_H__
#define __ADL_N_CADIZ_MEC172X_H__

extern uint8_t platformskutype;

#define KSC_PLAT_NAME                   "CDZ"

/* I2C addresses */
#define EEPROM_DRIVER_I2C_ADDR          0x50

/* Signal to gpio mapping for MEC172x based ADL-N is described here */

#define PM_SLP_SUS			EC_GPIO_000 // XXX

#define TOP_SWAP_STRAP			EC_GPIO_011
#define RSMRST_PWRGD_EC_N		EC_GPIO_012 // XXX
#define G3_SAF_DETECT			EC_GPIO_013 // XXX
#define PCH_DPWROK			EC_GPIO_016 //
#define MAINS_B_PWRGD			EC_GPIO_017 //

#define SOC_RSTBTN_N			EC_GPIO_020 // XXX
#define VCCIN_AUX_PWRGD			EC_GPIO_021 //
#define CPU_C10_GATE			EC_GPIO_022 // XXX
#define PS_ON_OUT			EC_GPIO_023 // XXX (PS_ON_UC)
#define PM_USB3A_B_PWR_EN		EC_GPIO_024 // XXX (EC_FLH_MUX_SEL)
#define GPIO_UC_2			EC_GPIO_025
#define UNDEFINED_0			EC_GPIO_026 // TBD FAN SLAVE ?
#define UNDEFINED_1			EC_GPIO_027 // TBD FAN SLAVE ?

#define UNDEFINED_2			EC_GPIO_030 // TBD FAN SLAVE ?
#define UNDEFINED_3 			EC_GPIO_031 // TBD FAN SLAVE ?
#define SEC_OVERRIDE_3V3		EC_GPIO_032
#define EXPB_GPIO3_EC			EC_GPIO_033 //
#define PM_UC_PG3_ENTRY			EC_GPIO_034 // XXX
#define GPIO_HOST_UC_2			EC_GPIO_036

#define EXPA_SLOT_SBY_PWREN		EC_GPIO_040 //
#define CATERR_EC_N			EC_GPIO_041
#define H_PECI				EC_GPIO_042 //
#define SYS_PWROK			EC_GPIO_043 // XXX

#define SCM_PRSNT_N			EC_GPIO_050 //
#define PLTRST_N			EC_GPIO_052 // XXX
#define PM_RSMRST			EC_GPIO_055 // XXX
#define ALL_SYS_PWRGD			EC_GPIO_057 // XXX

#define REDUNDANT_MAINS_N		EC_GPIO_060 //
#define ESPI_RESET_MAF			EC_GPIO_061 // XXX (ESPI_RESET_UC_N

#define SEC_OVERRIDE_1V8		EC_GPIO_100
#define PM_PWRBTN			EC_GPIO_101 // XXX
#define EC_SMI				EC_GPIO_102 // XXX
#define PCH_PWROK			EC_GPIO_106 // XXX
#define GPIO_HOST_UC_0			EC_GPIO_107

#define GPIO_HOST_UC_1			EC_GPIO_110
#define PM_UC_DPWROK			EC_GPIO_111
#define GPIO_UC_0			EC_GPIO_112
#define GPIO_UC_1			EC_GPIO_113
#define SCM_WAKE_SCI_N			EC_GPIO_114 // XXX
#define WAKE_SCI			SCM_WAKE_SCI_N
#define DNX_FORCE_RELOAD_EC		EC_GPIO_115

#define EXP_SLOT_SBY_PWREN		EC_GPIO_120 //
#define EXP_POWER_MODE			EC_GPIO_121 //
#define EXP_CONN_ID_0			EC_GPIO_122 //
#define EXP_CONN_ID_1			EC_GPIO_123 //
#define EXPA_CARD_PRSNT_N		EC_GPIO_124 //
#define EXPB_CARD_PRSNT_N		EC_GPIO_125 //
#define POE_INT_N			EC_GPIO_127

#define SLOT0_LED_OUT_N			EC_GPIO_130
#define SLOT1_LED2_OUT			EC_GPIO_131
#define MAINS_A_PWRGD			EC_GPIO_132 //
#define BUTTON_PROTRUDING_N		EC_GPIO_135

#define PM_BATLOW			EC_GPIO_140 // XXX

#define PM_UC_DPWREN			EC_GPIO_151
#define FALLING_12V_N			EC_GPIO_155 // XXX

#define PROCHOT				EC_GPIO_160 // XXX
#define MIPI60_PWRBTN_N			EC_GPIO_162 // XXX
#define DBG_RST_N_MECC			EC_GPIO_165
#define BTN_RECESSED			EC_GPIO_166 // XXX

#define SLP_S4_N			EC_GPIO_172 // XXX
#define THRMTRIP_3P3_N			EC_GPIO_173 //

#define SLP_S3_N			EC_GPIO_220 // XXX
#define BOOT_MEDIA_PWR_CNTRL		EC_GPIO_221 // XXX
#define FAN_CTRLR_CONTROL		EC_GPIO_223 // 
#define PCHHOT_N			EC_GPIO_225
#define RSMRST_PWRGD			EC_GPIO_227 // XXX

#define FAN_CTRLR_RST_N			EC_GPIO_232 //
#define SLOT0_SSD_PLN_N			EC_GPIO_235 //

#define EC_CORE_GT_PE			EC_GPIO_240 // XXX
#define SX_EXIT_HOLDOFF_N		EC_GPIO_241 // XXX
#define SLOT1_LED1_OUT			EC_GPIO_242
#define SLOT0_SSD_PWRDIS		EC_GPIO_243 //
#define USB012_PWREN			EC_GPIO_244
#define PM_USB3A_T_PWR_EN		EC_GPIO_245 //
#define EXPB_SLOT1_MAIN_PWREN		EC_GPIO_246

#define EC_IGNITION_IN			EC_GPIO_254 //
#define EXPB_SLOT_SBY_PWREN		EC_GPIO_255

#define PWRBTN_EC_IN_N			BUTTON_PROTRUDING_N

#define PM_DS3				EC_DUMMY_GPIO_HIGH
#define TIMEOUT_DISABLE			EC_DUMMY_GPIO_HIGH
#define FAN_PWR_DISABLE_N		EC_DUMMY_GPIO_LOW
//#define THERM_STRAP			EC_DUMMY_GPIO_HIGH

/* EC GPIOS */
#define THERM_STRAP			EC_DUMMY_GPIO_HIGH // GPIO_UC_2 (for when the PU/PD works)
#define EC_PWRBTN_LED			EC_DUMMY_GPIO_HIGH
#define BC_ACOK				EC_DUMMY_GPIO_HIGH
#define DG2_PRESENT			EC_DUMMY_GPIO_LOW
#define PEG_RTD3_COLD_MOD_SW_R		EC_DUMMY_GPIO_LOW

/* Device instance names */
#define I2C_BUS_0			DT_NODELABEL(i2c_smb_0)
#define I2C_BUS_1			DT_NODELABEL(i2c_smb_1)
#define ESPI_0				DT_NODELABEL(espi0)
#define SPI_0				DT_NODELABEL(spi0)
#define ADC_CH_BASE			DT_NODELABEL(adc0)
#define PECI_0_INST			DT_NODELABEL(peci0)
#define WDT_0				DT_NODELABEL(wdog)
#define PCR				DT_NODELABEL(pcr)

/* Button/Switch Initial positions */
#define PWR_BTN_INIT_POS		1
#define BTN_RECESSED_INIT_POS		1

/* PD version: MSB byte represent Major version and
 * LSB byte represent Minor version
 */
#define USB_PD_VERSION 0x0200

#endif /* __ADL_N_CADIZ_MEC172X_H__ */
