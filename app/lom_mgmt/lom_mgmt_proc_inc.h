#ifndef __LOM_MGMT_PROC_INC_H__
#define __LOM_MGMT_PROC_INC_H__

#include "lom_mgmt_i2c.h"

void avail_res_postcode_set(int val);
void avail_res_event_set(int val);

#define TIMESTAMP_SZ 4

#define _DBG_PCODE
#define _DBG_EVENT

#if !defined(_DBG_EVENT) || (CONFIG_LOM_MGMT_PROC_LOG_LEVEL < LOG_LEVEL_DBG)
#define LOG_DBG_EVENT(...) (void)0
#else
#define LOG_DBG_EVENT(...) LOG_INF(__VA_ARGS__)
#endif

#if !defined(_DBG_PCODE) || (CONFIG_LOM_MGMT_PROC_LOG_LEVEL < LOG_LEVEL_DBG)
#define LOG_DBG_PCODE(...) (void)0
#else
#define LOG_DBG_PCODE(...) LOG_INF(__VA_ARGS__)
#endif

#endif /* __LOM_MGMT_PROC_INC_H__ */
