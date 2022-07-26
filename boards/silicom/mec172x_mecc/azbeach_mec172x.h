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

#define PLATFORM_DATA(x, y)  ((x) | ((y) << 8))

/* In ADL board id is 6-bits */
#define BOARD_ID_MASK        0x003Fu
#define BOM_ID_MASK          0x01C0u
#define FAB_ID_MASK          0x0600u
#define HW_STRAP_MASK        0xF800u
#define HW_ID_MASK           (FAB_ID_MASK|BOM_ID_MASK|BOARD_ID_MASK)
#define BOARD_ID_OFFSET      0u
#define BOM_ID_OFFSET        6u
#define FAB_ID_OFFSET        9u
#define HW_STRAP_OFFSET      11u


/* Support adl skus */
enum platform_skus {
	PLATFORM_ADL_P_SKUs = 0,
	PLATFORM_ADL_M_SKUs = 1,
	PLATFORM_ADL_N_SKUs = 2,
};

/* Support board ids */
#define BRD_ID_ADL_P_ERB		0x10u
#define BRD_ID_ADL_P_LP4_T3		0x10u
#define BRD_ID_ADL_P_LP4_T3_HSIO		0x11u
#define BRD_ID_ADL_P_DDR5_T3		0x12u
#define BRD_ID_ADL_P_LP5_T4		0x13u
#define BRD_ID_ADL_P_DDR4_T3		0x14u
#define BRD_ID_ADL_P_LP5_T3		0x15u
#define BRD_ID_ADL_P_LP4_BEP		0x19u
#define BRD_ID_ADL_P_LP5_AEP		0x1Au
#define BRD_ID_ADL_P_GCS		0x1Bu
#define BRD_ID_ADL_P_DG2_384EU_AEP		0x1Eu
#define BRD_ID_ADL_P_DG2_128EU_AEP		0x1Fu

#define BRD_ID_ADL_M_LP4		0x01u
#define BRD_ID_ADL_M_LP5		0x02u
#define BRD_ID_ADL_M_LP5_2PMIC		0x03u
#define BRD_ID_ADL_M_RVP2A_PPV		0x04u

#define BRD_ID_ADL_N_ERB		0x06u
#define BRD_ID_ADL_N_LP5		0x07u
#define BRD_ID_ADL_N_DDR4		0x08u
#define BRD_ID_ADL_N_DDR5		0x09u

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
#define SMC_LID				EC_GPIO_033
#define PM_BAT_STATUS_LED2		EC_GPIO_035
#define PEG_PLI_N_DG2			EC_GPIO_036
#define SYS_PWROK			EC_GPIO_043
#define STD_ADP_PRSNT			EC_GPIO_052

#define PM_RSMRST_G3SAF_P		EC_GPIO_054
#define PM_RSMRST_MAF_P			EC_GPIO_055
#define PM_RSMRST			((boot_mode_maf == 1) ? \
					 PM_RSMRST_MAF_P : \
					 PM_RSMRST_G3SAF_P)

#define ALL_SYS_PWRGD			EC_GPIO_057
#define FAN_PWR_DISABLE_N		EC_GPIO_060
/* We poll this GPIO in MAF mode in order to sense the input signal040_076.
 * This pin was already configured in pinmux as ALT mode 1 NOT GPIO
 */
#define ESPI_RESET_MAF			EC_GPIO_061
#define PCA9555_1_R_INT_N		EC_GPIO_062
#define EC_SLATEMODE_HALLOUT_SNSR_R	EC_GPIO_064

#define PM_PWRBTN			EC_GPIO_101
#define EC_SMI				EC_GPIO_102
#define PCH_PWROK			EC_GPIO_106
#define WAKE_SCI			EC_GPIO_114
#define DNX_FORCE_RELOAD_EC		EC_GPIO_115
#define PM_BATLOW			EC_GPIO_140
#define CATERR_LED_DRV			EC_GPIO_153
#define CS_INDICATE_LED			EC_GPIO_156		/* ADL-M */
#define C10_GATE_LED			EC_GPIO_157		/* ADL-M */
#define RST_MECC			EC_GPIO_165		/* ADL-M */
#define SX_EXIT_HOLDOFF_N		EC_GPIO_175

#define PM_SLP_S0_CS			EC_GPIO_221
#define PM_SLP_S0_EC_N			EC_GPIO_240		/* ADL-M */
#define EC_PWRBTN_LED			EC_GPIO_014		/* FP Left Blue */

/* Device instance names */
#define I2C_BUS_0			DT_LABEL(DT_NODELABEL(i2c_smb_0))
#define ESPI_0				DT_LABEL(DT_NODELABEL(espi0))
#define SPI_0				DT_LABEL(DT_NODELABEL(spi0))
#define ADC_CH_BASE			DT_LABEL(DT_NODELABEL(adc0))
#define PECI_0_INST			DT_LABEL(DT_NODELABEL(peci0))
#define WDT_0				DT_LABEL(DT_NODELABEL(wdog))

/* Button/Switch Initial positions */
#define PWR_BTN_INIT_POS		1

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
