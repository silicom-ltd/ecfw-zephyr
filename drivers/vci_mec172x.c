/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <errno.h>
#include <zephyr/logging/log.h>
#include "board.h"
//#include "vci_mec172x.h"

LOG_MODULE_REGISTER(vci_mec, CONFIG_VCI_EC_LOG_LEVEL);

/* VCI Config register */
#define MCHP_VCI_CFG_REG_OFS            0
#define MCHP_VCI_CFG_REG_MASK           0x00071f8fu
#define MCHP_VCI_CFG_IN03_MASK          0x0fu
#define MCHP_VCI_CFG_IN0_HI             0x01u
#define MCHP_VCI_CFG_IN1_HI             0x02u
#define MCHP_VCI_CFG_IN2_HI             0x04u
#define MCHP_VCI_CFG_IN3_HI             0x08u
#define MCHP_VCI_CFG_VPWR_POS           7
#define MCHP_VCI_CFG_VPWR_VTR           0
#define MCHP_VCI_CFG_VPWR_VBAT          BIT(7)
#define MCHP_VCI_VCI_OVRD_IN_PIN        BIT(8)
#define MCHP_VCI_VCI_OUT_PIN            BIT(9)
#define MCHP_VCI_FW_CTRL_EN             BIT(10)
#define MCHP_VCI_FW_EXT_SEL             BIT(11)
#define MCHP_VCI_FILTER_BYPASS          BIT(12)
#define MCHP_VCI_WEEK_ALARM             BIT(16)
#define MCHP_VCI_RTC_ALARM              BIT(17)
#define MCHP_VCI_SYS_PWR_PRES           BIT(18)

/* VCI Latch Enable register */
#define MCHP_VCI_LE_IN03_MASK           0x0fu
#define MCHP_VCI_LE_IN0                 0x01u
#define MCHP_VCI_LE_IN1                 0x02u
#define MCHP_VCI_LE_IN2                 0x04u
#define MCHP_VCI_LE_IN3                 0x08u
#define MCHP_VCI_LE_WEEK_ALARM          BIT(16)
#define MCHP_VCI_LE_RTC_ALARM           BIT(17)

/* VCI Latch Resets register */
#define MCHP_VCI_LR_IN03_MASK           0x0fu
#define MCHP_VCI_LR_IN0                 0x01u
#define MCHP_VCI_LR_IN1                 0x02u
#define MCHP_VCI_LR_IN2                 0x04u
#define MCHP_VCI_LR_IN3                 0x08u
#define MCHP_VCI_LR_WEEK_ALARM          BIT(16)
#define MCHP_VCI_LR_RTC_ALARM           BIT(17)

/* VCI Input Enable register */
#define MCHP_VCI_INPUT_EN_IE30_MASK     0x0fu
#define MCHP_VCI_INPUT_EN_IN0           0x01u
#define MCHP_VCI_INPUT_EN_IN1           0x02u
#define MCHP_VCI_INPUT_EN_IN2           0x04u
#define MCHP_VCI_INPUT_EN_IN3           0x08u

/* VCI Polarity register */
#define MCHP_VCI_POL_IE30_MASK          0x0fu
#define MCHP_VCI_POL_ACT_HI_IN0         0x01u
#define MCHP_VCI_POL_ACT_HI_IN1         0x02u
#define MCHP_VCI_POL_ACT_HI_IN2         0x04u
#define MCHP_VCI_POL_ACT_HI_IN3         0x08u

/* VCI Positive Edge Detect register */
#define MCHP_VCI_PDET_IE30_MASK         0x0fu
#define MCHP_VCI_PDET_IN0               0x01u
#define MCHP_VCI_PDET_IN1               0x02u
#define MCHP_VCI_PDET_IN2               0x04u
#define MCHP_VCI_PDET_IN3               0x08u

/* VCI Positive Edge Detect register */
#define MCHP_VCI_NDET_IE30_MASK         0x0fu
#define MCHP_VCI_NDET_IN0               0x01u
#define MCHP_VCI_NDET_IN1               0x02u
#define MCHP_VCI_NDET_IN2               0x04u
#define MCHP_VCI_NDET_IN3               0x08u

/* VCI Buffer Enable register */
#define MCHP_VCI_BEN_IE30_MASK          0x0fu
#define MCHP_VCI_BEN_IN0                0x01u
#define MCHP_VCI_BEN_IN1                0x02u
#define MCHP_VCI_BEN_IN2                0x04u
#define MCHP_VCI_BEN_IN3                0x08u

struct vci {
	uint32_t config;
	uint32_t latch_en;
	uint32_t latch_rst;
	uint32_t input_en;
	uint32_t hold_off;
	uint32_t polarity;
	uint32_t pedge_det;
	uint32_t nedge_det;
	uint32_t buffer_en;
};

#define DT_DRV_COMPAT microchip_xec_vci_v2
int vci_init(void)
{
	struct vci *vci_regs = (struct vci *)DT_INST_REG_ADDR(0);

	LOG_ERR("Entering VCI init\n");
	/* Set polarity for VCI_IN0 as active low */
	vci_regs->polarity &= ~MCHP_VCI_POL_ACT_HI_IN0;

	vci_regs->input_en |= MCHP_VCI_INPUT_EN_IN0;

	/* Reset the VCI_IN# latches before enable */
	vci_regs->latch_rst |= MCHP_VCI_LR_IN0;

	/* Enable latches on VCI_IN0 so that assertions will be latched
	 * until next reset
	 */
#if 0
	vci_regs->latch_en |= (MCHP_VCI_LE_IN0 | MCHP_VCI_LE_IN1);
#endif

	/* Enable Input Filters on VCI_IN# pins */
	vci_regs->config &= ~MCHP_VCI_FW_EXT_SEL;
	vci_regs->config &= ~MCHP_VCI_FILTER_BYPASS;

	vci_regs->config |= MCHP_VCI_CFG_IN0_HI;

	vci_regs->buffer_en &= ~MCHP_VCI_BEN_IN0;

	/* Configure VCI_OUT for FW powered */
	vci_regs->config |= MCHP_VCI_FW_CTRL_EN; /* set output to 1 */
	vci_regs->config |= MCHP_VCI_FW_EXT_SEL; /* set to FW control */

	return 0;
}
#undef DT_DRV_COMPAT
