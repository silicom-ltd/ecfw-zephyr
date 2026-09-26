/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LOM_MGMT_PROC_INC_H__
#define __LOM_MGMT_PROC_INC_H__

#include "lom_mgmt_i2c.h"

void avail_resource_set(int bit, int val);

#define TIMESTAMP_SZ 4

#ifdef CONFIG_LOM_MGMT_PROC_DBG_APP
#define LOG_DBG_APP(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_APP(...) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_PROC_DBG_EVENT
#define LOG_DBG_EVENT(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_EVENT(...) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_PROC_DBG_PCODE
#define LOG_DBG_PCODE(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_PCODE(...) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_PROC_DBG_SENS
#define LOG_DBG_SENS(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_SENS(...) (void)0
#endif

#ifdef CONFIG_LOM_MGMT_PROC_DBG_PWRCTRL
#define LOG_DBG_PWRCTRL(...) LOG_INF(__VA_ARGS__)
#else
#define LOG_DBG_PWRCTRL(...) (void)0
#endif

#endif /* __LOM_MGMT_PROC_INC_H__ */
