/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_COMMON_H__
#define __BOARD_COMMON_H__

/**
 * @brief This global variable helps to configure variable gpios.
 */
extern uint8_t boot_mode_maf;

#if defined(CONFIG_SOC_FAMILY_MEC)

#ifdef CONFIG_BOARD_MEC1501MODULAR_ASSY6885
#include "mec15xx_aic_rvp_selection.h"
#elif CONFIG_BOARD_MEC172XMODULAR_ASSY6930
#include "mecc172x_aic_rvp_selection.h"
#elif defined(CONFIG_BOARD_MEC1501_ADL)
#include "adl_mec1501.h"
#elif defined(CONFIG_BOARD_MEC1501_ADL_P)
#include "adl_p_mec1501.h"
#elif defined(CONFIG_BOARD_MEC172X_AZBEACH)
#include "azbeach_mec172x.h"
#elif defined(CONFIG_BOARD_MEC172X_ADL_N)
#include "adl_n_mec172x.h"
#elif defined(CONFIG_BOARD_MEC172X_ADL_N_CP)
#include "adl_n_cp_mec172x.h"
#else
#error "Platform not supported"
#endif /* CONFIG_BOARD_MEC1501MODULAR_ASSY6885 */

#endif /* CONFIG_SOC_FAMILY_MEC */

#if defined(CONFIG_THERMAL_MANAGEMENT) || defined(CONFIG_THERMAL_MANAGEMENT_V2)
#include "thermalmgmt.h"
#include "board_thermal.h"
#elif defined(CONFIG_THERMAL_MANAGEMENT_V4) || \
	defined(CONFIG_THERMAL_MANAGEMENT_V5)
/* V4 (and V5, copied from it) get their sensor and fan devices from the
 * devicetree, so they do not use the V1/V2 board table API in
 * board_thermal.h - and that header's prototypes reference struct
 * therm_sensor / struct fan_dev, which V4/V5 do not define.
 * Pull in thermalmgmt.h alone so the SMC host can reach the V4/V5 hooks.
 */
#include "thermalmgmt.h"
#endif
/**
 * @brief Perform platform configuration depending on the board.
 *
 * @retval 0 If successful, otherwise negative error code.
 */
int board_init(void);

/**
 * @brief Perform platform configuration during suspend depending on the board.
 *
 * Note: Allows to optimize power consumption while the system is in S3/S4/S5.
 *
 * @retval 0 If successful, otherwise negative error code.
 */
int board_suspend(void);

/**
 * @brief Perform platform configuration during resume depending on the board.
 *
 * Note: Allows to restore pin functionality when the system exits S3/S4/S5.
 *
 * @retval 0 If successful, otherwise negative error code.
 */
int board_resume(void);

#endif /* __BOARD_COMMON_H__ */
