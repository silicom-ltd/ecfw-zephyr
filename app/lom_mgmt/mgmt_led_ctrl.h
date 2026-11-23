#ifndef _LOM_MGMT_FUNC_MGMT_LED_CTRL_H_
#define _LOM_MGMT_FUNC_MGMT_LED_CTRL_H_

#ifdef CONFIG_LOM_MGMT_FUNC_MGMT_LED_CTRL

#include <stdint.h>

enum mgmt_led_id {
	MGMT_LED_A, /* led amber */
	MGMT_LED_R, /* led red */
	MGMT_LED_G, /* led green */
	MGMT_LED_B, /* led blue */
	MGMT_LED_NUM,
};

#define MGMT_LED_OP_SET_COLOR 0
#define MGMT_LED_OP_OFF       1
#define MGMT_LED_OP_BLINK     2

int mgmt_led_is_ready(void);
int mgmt_led_off(void);
int mgmt_led_set_color(uint8_t led_id);
int mgmt_led_blink(uint8_t led_id, uint32_t delay_on, uint32_t delay_off);

#endif /* CONFIG_LOM_MGMT_FUNC_MGMT_LED_CTRL */

#endif /* _LOM_MGMT_FUNC_MGMT_LED_CTRL_H_ */
