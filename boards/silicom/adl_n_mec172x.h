/*
 * Copyright (c) 2020 Intel Corporation
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include "mec172x_pin.h"
#include "common_mec172x.h"

#ifndef __ADL_N_MEC172X_H__
#define __ADL_N_MEC172X_H__

extern uint8_t platformskutype;

#define KSC_PLAT_NAME                   "IBZ"

/* I2C addresses */
#define EEPROM_DRIVER_I2C_ADDR          0x50

/* Signal to gpio mapping for MEC172x based ADL-N is described here */

/* power/boot related */
#define PM_SLP_SUS			EC_GPIO_000 // XXX
#define RSMRST_PWRGD_G3SAF_P		EC_GPIO_012 // XXX
#define G3_SAF_DETECT			EC_GPIO_013 // XXX
#define SOC_RSTBTN_N			EC_GPIO_020 // XXX
#define CPU_C10_GATE			EC_GPIO_022 // XXX
#define PS_ON_OUT			EC_GPIO_023 // XXX (PS_ON_UC)
#define PM_UC_PG3_ENTRY			EC_GPIO_034 // XXX
#define SYS_PWROK			EC_GPIO_043 // XXX
#define PVT_SPI_BOOT			EC_GPIO_045 // XXX (UC_PVT_SPI_SELECT
#define PLTRST_N			EC_GPIO_052 // XXX
#define PM_RSMRST_G3SAF_P 		EC_GPIO_253 // XXX (PM_RSMRST_N)
#define ALL_SYS_PWRGD			EC_GPIO_057 // XXX
/* We poll this GPIO in MAF mode in order to sense the input signal040_076.
 * This pin was already configured in pinmux as ALT mode 1 NOT GPIO
 */
#define ESPI_RESET_MAF			EC_GPIO_061 // XXX (ESPI_RESET_UC_N
#define PM_PWRBTN			EC_GPIO_101 // XXX
#define PCH_PWROK			EC_GPIO_106 // XXX
#define PM_BATLOW			EC_GPIO_140 // XXX
#define SLP_S3_N			EC_GPIO_220 // XXX
#define SLP_S4_N			EC_GPIO_172 // XXX
#define SX_EXIT_HOLDOFF_N		EC_GPIO_175 // XXX
#define RSMRST_PWRGD			EC_GPIO_227 // XXX
#define PM_RSMRST			EC_GPIO_055 // XXX

#define EC_PCH_SPI_OE_N			EC_GPIO_024 // XXX (EC_FLH_MUX_SEL)

/* button/peripherals */
#define BTN_PROTRUDING			EC_GPIO_135 // XXX
#define MIPI60_PWRBTN_N			EC_GPIO_162 // XXX
#define BTN_RECESSED			EC_GPIO_166 // XXX
#define PWRBTN_EC_IN_N			BTN_PROTRUDING

#define EC_SMI				EC_GPIO_102 // XXX
#define WAKE_SCI			EC_GPIO_114 // XXX
#define DNX_FORCE_RELOAD_EC		EC_GPIO_115 // XXX
#define CATERR_LED_DRV			EC_GPIO_153 // XXX (LED_FP_AMBER_BTM_N)
#define PM_DS3				EC_DUMMY_GPIO_HIGH
#define TIMEOUT_DISABLE			EC_DUMMY_GPIO_LOW
#define FAN_PWR_DISABLE_N		EC_DUMMY_GPIO_LOW
//#define THERM_STRAP			EC_DUMMY_GPIO_HIGH

/* (unused/DNI) */
#define PM_SLP_S0_CS			EC_GPIO_221 // XXX
#define EC_PWRBTN_LED			EC_DUMMY_GPIO_LOW

/* Host VR PE control */
#define EC_CORE_SA_PE			EC_GPIO_240 // XXX

/* M.2 Slots */
#define W_DISABLE_M2_SLOT1_N		EC_GPIO_026 // XXX
#define W_DISABLE_M2_SLOT2_N		EC_GPIO_027 // XXX
#define W_DISABLE_M2_SLOT3_N		EC_GPIO_060 // XXX
#define SIM_M2_SLOT3_MUX_SEL		EC_GPIO_030 // XXX
#define SIM_M2_SLOT2_MUX_SEL		EC_GPIO_031 // XXX
#define SLOT0_LED1_OUT			EC_GPIO_130 // XXX
#define SLOT0_LED2_OUT			EC_GPIO_131 // XXX
#define SIM_M2_SLOT2A_DET_N		EC_GPIO_152 // XXX
#define FALLING_12V_N			EC_GPIO_155 // XXX
//#define SLOT3_SSD_PWRDIS		EC_GPIO_220
#define RST_CTL_M2_SLOT1_N		EC_GPIO_226 // XXX
#define RST_CTL_M2_SLOT2_N		EC_GPIO_033 // XXX
#define RST_CTL_M2_SLOT3_N		EC_GPIO_241 
#define SIM_M2_SLOT2B_DET_N		EC_GPIO_232 // XXX
#define SLOT1_LED_OUT			EC_GPIO_242 // XXX
#define SLOT2_LED_OUT			EC_GPIO_243 // XXX
#define SIM_M2_SLOT3A_DET_N		EC_GPIO_154 // XXX
#define SIM_M2_SLOT3B_DET_N		EC_GPIO_246 // XXX
#define SIM_M2_SLOT2A_DET_N		EC_GPIO_152
#define SIM_M2_SLOT2B_DET_N		EC_GPIO_232
#define SIM_M2_SLOT3_MUX_SEL		EC_GPIO_030
#define SIM_M2_SLOT2_MUX_SEL		EC_GPIO_031

/* GPIO HOST */
#define GPIO_HOST_UC_0			EC_GPIO_107
#define GPIO_HOST_UC_1			EC_GPIO_110
#define GPIO_HOST_UC_2			EC_GPIO_036
#define SEC_OVERRIDE_1V8		EC_GPIO_100
#define TOP_SWAP_STRAP			EC_GPIO_011
#define SEC_OVERRIDE_3V3		EC_GPIO_032
#define CATERR_EC_N			EC_GPIO_041

/* EC GPIOS */
#define GPIO_UC_0			EC_GPIO_112
#define GPIO_UC_1			EC_GPIO_113
#define GPIO_UC_2			EC_GPIO_025
#define THERM_STRAP			EC_DUMMY_GPIO_HIGH // GPIO_UC_2 (for when the PU/PD works)

/* EXP port */
#define EXP_PWREN_EC			EC_GPIO_040 

#define BC_ACOK				EC_DUMMY_GPIO_HIGH
#define DG2_PRESENT			EC_DUMMY_GPIO_LOW
#define PEG_RTD3_COLD_MOD_SW_R		EC_DUMMY_GPIO_LOW

#define PROCHOT				EC_GPIO_160 // XXX

/* USB3 port enable */
#define PM_USB3A_PWR_EN			EC_GPIO_245

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

// struct sbl version for LOM
struct sbl_version {
    uint8_t image_id[8];
    uint8_t major_version;
    uint8_t minor_version;
    uint8_t build_number_high;
    uint8_t build_number_low;
    uint8_t flags;
            //flags:  [3:0]reserved, [4]image_arch,
            //        [5]bld_debug,  [6]fsp_debug,
            //        [7]dirty
};

/**
    struct sys_ids holds all the members of struct sbl_version
    e.g image id, major, minor, bld_number_high and bld_number_low, flags,
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
void get_sbl_version(struct sbl_version *sbl);

/**
    LOM IP address (IPv4 or IPv6), reported to the host on request.
    The address octets are stored in network byte order (octet[0] = most
    significant). Populated by the (separately handled) LOM management
    transport; defaults to the "not reported" sentinel until the LOM
    reports an address.

    The address is reported to the host as a self-describing, variable
    length response:

        byte 0   : family  - LOM_IP_FAMILY_V4 / _V6 / _NONE
        byte 1   : prefix  - subnet mask as a CIDR bit count
                             (0-32 for IPv4, 0-128 for IPv6)
        byte 2.. : address - octets in network order, LOM_IPV4_ADDR_LEN
                             (4) or LOM_IPV6_ADDR_LEN (16) bytes; absent
                             when family is LOM_IP_FAMILY_NONE
**/
#define LOM_IP_FAMILY_NONE 0xFF
#define LOM_IP_FAMILY_V4   0x04
#define LOM_IP_FAMILY_V6   0x06

#define LOM_IPV4_ADDR_LEN  4
#define LOM_IPV6_ADDR_LEN  16

/* Largest response: family(1) + prefix(1) + IPv6 address(16). */
#define LOM_IP_RESP_MAX    (2 + LOM_IPV6_ADDR_LEN)

void set_lom_ip (uint8_t family, uint8_t prefix, const uint8_t *addr, uint8_t len);
uint8_t get_lom_ip_resp (uint8_t *out);

#endif /* __AZBEACH_MEC172X_H__ */
