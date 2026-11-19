#ifdef CONFIG_LOM_MGMT_FUNC_MGMT_LED_CTRL

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>

#include "lom_mgmt_proc_inc.h"
#include "mgmt_led_ctrl.h"

LOG_MODULE_DECLARE(lom_mgmt, CONFIG_LOM_MGMT_PROC_LOG_LEVEL);

#define MGMT_LEDS_NODE DT_NODELABEL(mgmt_leds)

#if !DT_NODE_EXISTS(MGMT_LEDS_NODE) || !DT_NODE_HAS_STATUS(MGMT_LEDS_NODE, okay)
#error "mgmt_leds node is missing from devicetree or be disabled!"
#endif

static const struct device *mgmt_led_dev = DEVICE_DT_GET(MGMT_LEDS_NODE);

int mgmt_led_is_ready()
{
	if (!device_is_ready(mgmt_led_dev)) {
		LOG_ERR("mgmt_led device is not ready");
		return 0;
	}

	return 1;
}

int mgmt_led_off(void)
{
	int ret;

	for (unsigned i = 0; i < MGMT_LED_NUM; i++) {
		ret = led_off(mgmt_led_dev, i);
		if (ret) {
			LOG_ERR("mgmt_led[%d] off failed: %d", i, ret);
			return ret;
		}
	}

	return 0;
}

int mgmt_led_set_color(uint8_t led_id)
{
	int ret;

	if (led_id >= MGMT_LED_NUM) {
		LOG_ERR("Invalid MGMT_LED_CTRL request");
		return -EINVAL;
	}

	LOG_DBG_APP("mgmt_led set color: %d", led_id);

	ret = mgmt_led_off();
	if (ret) {
		return ret;
	}

	ret = led_set_brightness(mgmt_led_dev, led_id, 100);
	if (ret) {
		LOG_ERR("mgmt_led[%d] set brightness failed: %d", led_id, ret);
		return ret;
	}

	return 0;
}

int mgmt_led_blink(uint8_t led_id, uint32_t delay_on, uint32_t delay_off)
{
#ifdef CONFIG_LED_GPIO_BLINK
	int ret;

	if (led_id >= MGMT_LED_NUM) {
		LOG_ERR("Invalid MGMT_LED_CTRL request");
		return -EINVAL;
	}

	LOG_DBG_APP("mgmt_led[%d] blink: delay_on %u(ms), delay_off %u(ms)", led_id,
		delay_on, delay_off);

	ret = mgmt_led_off();
	if (ret) {
		return ret;
	}

	ret = led_blink(mgmt_led_dev, led_id, delay_on, delay_off);
	if (ret) {
		LOG_ERR("mgmt_led[%d] blink failed: %d", led_id, ret);
		return ret;
	}
#endif

	return 0;
}

#endif
