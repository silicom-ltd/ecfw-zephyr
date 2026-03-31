#ifndef __LOM_MGMT_PROC_INC_H__
#define __LOM_MGMT_PROC_INC_H__

#include "lom_mgmt_i2c.h"

void avail_resource_set(int bit, int val);

#define TIMESTAMP_SZ 4

/*
 * LOM_MGMT_PROC's log switches
 */
//#define _DBG_APP
//#define _DBG_PCODE
#define _DBG_EVENT
//#define _DBG_SENS

#if !defined(_DBG_APP) || (CONFIG_LOM_MGMT_PROC_LOG_LEVEL < LOG_LEVEL_DBG)
#define LOG_DBG_APP(...) (void)0
#else
#define LOG_DBG_APP(...) LOG_INF(__VA_ARGS__)
#endif

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

#if !defined(_DBG_SENS) || (CONFIG_LOM_MGMT_PROC_LOG_LEVEL < LOG_LEVEL_DBG)
#define LOG_DBG_SENS(...) (void)0
#else
#define LOG_DBG_SENS(...) LOG_INF(__VA_ARGS__)
#endif

#ifndef CONFIG_BOARD_MEC172X_ADL_N_CP
enum sensor_types {
	hwmon_temp,
	hwmon_in,
	hwmon_curr,
	hwmon_power,
	hwmon_energy,
	hwmon_fan,
	hwmon_pwm,
};
#endif

#endif /* __LOM_MGMT_PROC_INC_H__ */
