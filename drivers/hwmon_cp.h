/*
 * Copyright (c) 2024 Silicom Connectivity Solutions Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __HWMON_CP_H__
#define __HWMON_CP_H__

#define SW_THERMAL_SENSOR_NUM ( \
		DT_NUM_INST_STATUS_OKAY(mps_mpq8785_temp) +	\
		DT_NUM_INST_STATUS_OKAY(ocp_crps_temp) +	\
		DT_NUM_INST_STATUS_OKAY(mps_mp2928_temp)	\
	)

#define SW_VOLTAGE_SENSOR_NUM (					\
		DT_NUM_INST_STATUS_OKAY(mps_mpq8785_vin) +	\
		DT_NUM_INST_STATUS_OKAY(mps_mpq8785_vout) +	\
		DT_NUM_INST_STATUS_OKAY(mps_mp2928_vout)	\
	)

#define SW_CURRENT_SENSOR_NUM (					\
		DT_NUM_INST_STATUS_OKAY(mps_mpq8785_iout) +	\
		DT_NUM_INST_STATUS_OKAY(ocp_crps_iout)		\
	)

#endif	/* __HWMON_CP_H__ */
