#ifndef __LOM_MGMT_PROC_INC_H__
#define __LOM_MGMT_PROC_INC_H__

#include "lom_mgmt_i2c.h"

#define AVAIL_RES_POSTCODE_SET(val)			\
	lom_mgmt_i2c_set_avail_res(lom_mgmt_dev,	\
		AVAIL_RES_MASK_POSTCODE,		\
		AVAIL_RES_BIT_VAL_POSTCODE(val))

#define AVAIL_RES_EVENT_SET(val)			\
	lom_mgmt_i2c_set_avail_res(lom_mgmt_dev,	\
		AVAIL_RES_MASK_EVENT,			\
		AVAIL_RES_BIT_VAL_EVENT(val))

#endif /* __LOM_MGMT_PROC_INC_H__ */
