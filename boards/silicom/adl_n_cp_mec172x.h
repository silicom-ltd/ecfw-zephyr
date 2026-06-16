/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024-2025 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include "mec172x_pin.h"
#include "common_mec172x.h"

#ifndef __ADL_N_MEC172X_CP_H__
#define __ADL_N_MEC172X_CP_H__

extern uint8_t platformskutype;

#define KSC_PLAT_NAME                   "CKP"

/* I2C addresses */
#define EEPROM_DRIVER_I2C_ADDR          0x50

/* Signal to gpio mapping for MEC172x based ADL-N is described here */

#define PM_SLP_SUS			EC_GPIO_000 // XXX

#define TOP_SWAP_STRAP			EC_GPIO_011
#define RSMRST_PWRGD_G3SAF_P		EC_GPIO_012 // XXX
#define G3_SAF_DETECT			EC_GPIO_013 // XXX
#define PCH_DPWROK			EC_GPIO_016 //
#define LOM_HM_SMI_N			EC_GPIO_017 //

#define SOC_RSTBTN_N			EC_GPIO_020 // XXX
#define VCCIN_AUX_PWRGD			EC_GPIO_021 //
#define CPU_C10_GATE			EC_GPIO_022 // XXX
#define PS_ON_OUT			EC_GPIO_023 // XXX (PS_ON_UC)
#define EC_PCH_SPI_OE_N			EC_GPIO_024 // XXX (EC_FLH_MUX_SEL)
#define GPIO_UC_2			EC_GPIO_025
#define W_DISABLE_M2_SLOT1_N		EC_GPIO_026 // XXX
#define LOM_READY			EC_GPIO_027 //

#define LOM_RESET_OUT_N			EC_GPIO_030 //
#define LOM_PWRBTN_N			EC_GPIO_031 //
#define SEC_OVERRIDE_3V3		EC_GPIO_032
#define FAN4_SENSE			EC_GPIO_033 //
#define PM_UC_PG3_ENTRY			EC_GPIO_034 // XXX
#define GPIO_HOST_UC_2			EC_GPIO_036

#define CATERR_EC_N			EC_GPIO_041
#define SYS_PWROK			EC_GPIO_043 // XXX
#define LOM_CARD_SENSE			EC_GPIO_046 //

#define FAN_CTRL1_ALERT			EC_GPIO_050 //
#define FAN3_SENSE			EC_GPIO_051 //
#define PLTRST_N			EC_GPIO_052 // XXX
#define PUSHBUT_UID			EC_GPIO_053 //
#define PM_RSMRST			EC_GPIO_055 // XXX
#define ALL_SYS_PWRGD			EC_GPIO_057 // XXX

#define LOM_RST_N			EC_GPIO_060 //
#define ESPI_RESET_MAF			EC_GPIO_061 // XXX (ESPI_RESET_UC_N
#define ENABLE_12V_FAN_N		EC_GPIO_062 //

#define SEC_OVERRIDE_1V8		EC_GPIO_100
#define PM_PWRBTN			EC_GPIO_101 // XXX
#define EC_SMI				EC_GPIO_102 // XXX
#define PCH_PWROK			EC_GPIO_106 // XXX
#define GPIO_HOST_UC_0			EC_GPIO_107

#define GPIO_HOST_UC_1			EC_GPIO_110
#define GPIO_UC_1			EC_GPIO_113
#define WAKE_SCI			EC_GPIO_114 // XXX
#define EC_PWRBTN_LED			EC_GPIO_115

#define SW_PWR_ON_OFF			EC_GPIO_120 //
#define LOM_GPIO_IN0			EC_GPIO_121 //
#define LOM_GPIO_IN1			EC_GPIO_122 //
#define SW_GPIO_IN1			EC_GPIO_123 //
#define LOM_GPIO_OUT0			EC_GPIO_124 //
#define LOM_GPIO_OUT1			EC_GPIO_125 //
#define SW_GPIO_OUT1			EC_GPIO_126 //

#define RS232_USB_DET			EC_GPIO_132 //
#define BTN_PROTRUDING			EC_GPIO_135 // XXX

#define PM_BATLOW			EC_GPIO_140 // XXX

#define CPU_PWR_OK			EC_GPIO_152 //
#define SW_SENSE_N			EC_GPIO_154 //
#define FALLING_12V_N			EC_GPIO_155 // XXX

#define PROCHOT				EC_GPIO_160 // XXX
#define MIPI60_PWRBTN_N			EC_GPIO_162 // XXX
#define BTN_RECESSED			EC_GPIO_166 // XXX

#define SLP_S4_N			EC_GPIO_172 // XXX
#define THRMTRIP_3P3_N			EC_GPIO_173 //
#define SX_EXIT_HOLDOFF_N		EC_GPIO_175 // XXX

#define SLP_S3_N			EC_GPIO_220 // XXX
#define PM_SLP_S0_CS			EC_GPIO_221 // XXX
#define FAN1_SENSE			EC_GPIO_226 // XXX
#define RSMRST_PWRGD			EC_GPIO_227 // XXX

#define SW_PWR_OK			EC_GPIO_233 //
#define PM_P3V3_DSW_PG			EC_GPIO_235 //
#define UART_MUX_SEL			EC_GPIO_236 //

#define EC_CORE_SA_PE			EC_GPIO_240 // XXX
#define SLOT1_LED_OUT			EC_GPIO_242 // XXX
#define SW_RESET			EC_GPIO_243 //
#define PM_USB3A_PWR_EN			EC_GPIO_245 //
#define FAN2_SENSE			EC_GPIO_246

#define PM_RSMRST_G3SAF_P 		EC_GPIO_253 // XXX (PM_RSMRST_N)
#define W_DISABLE_M2_SLOT0_N		EC_GPIO_254 //

#define PWRBTN_EC_IN_N			BTN_PROTRUDING

#define PM_DS3				EC_DUMMY_GPIO_HIGH
#define TIMEOUT_DISABLE			EC_DUMMY_GPIO_HIGH
#define FAN_PWR_DISABLE_N		EC_DUMMY_GPIO_LOW
//#define THERM_STRAP			EC_DUMMY_GPIO_HIGH

/* EC GPIOS */
#define THERM_STRAP			EC_DUMMY_GPIO_HIGH // GPIO_UC_2 (for when the PU/PD works)

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

/* MAESTRO DDR4 and DDR5 bom ids, board id */
#define MAESTRO_DDR4_BOM_ID             0x04
#define MAESTRO_DDR5_BOM_ID             0x05
#define MAESTRO_BOARD_ID                0x15
#define MAESTRO_VTT_SODIMM_ADC_CHNL     14

// struct sbl version for LOM
struct sbl_version {
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t build_number_high;
    uint8_t build_number_low;
    uint8_t flags;
            //flags:    [3:0]reserved, [4]image_arch,
            //          [5]bld_debug,  [6]fsp_debug,
            //          [7]dirty
};

/**
    struct sys_ids holds all the members of struct sbl_version
    e.g major, minor, bld_number_high and bld_number_low, flags,
    and additionally board_id and bom_id
**/
struct sys_ids {
    struct sbl_version sbl;
    uint8_t board_id;
    uint8_t bom_id;
};

/**
    functions to set and get sbl version,
    board_id and bom_id
**/
void set_sys_ids (const uint8_t *pdata, uint8_t len);
uint8_t get_bom_id (void);
uint8_t get_board_id (void);
struct sbl_version *get_sbl_version(void);

#endif /* __AZBEACH_MEC172X_H__ */
