/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT microchip_xec_bgpo

#define LOG_LEVEL CONFIG_GPIO_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bgpo_mchp_xec);

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <soc.h>
#include <errno.h>

struct bgpo_xec_regs {
	uint32_t bgpo_data;
	uint32_t bgpo_pwr;
	uint32_t bgpo_rst;
};

struct bgpo_xec_config {
	struct bgpo_xec_regs *regs;
	const struct pinctrl_dev_config *pcfg;
};

static int bgpo_xec_init(const struct device *dev)
{
	const struct bgpo_xec_config *const cfg = dev->config;
	struct bgpo_xec_regs * const regs = cfg->regs;
	const struct pinctrl_state *state;
	
	uint32_t portpin, func, mux, port, pin;
	int ret;

	ret = pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		LOG_ERR("XEC BGPO pinctrl setup failed (%d)", ret);
		return ret;
	}
	
	ret = pinctrl_lookup_state(cfg->pcfg, 0, &state);
	for (uint8_t i = 0U; i < state->pin_cnt; i++) {
		mux = state->pins[i].pinmux;

		func = MCHP_XEC_PINMUX_FUNC(mux);
		if (func >= MCHP_AFMAX) {
			return -EINVAL;
		}
		port = MCHP_XEC_PINMUX_PORT(mux);
		portpin = MCHP_XEC_PINMUX_PIN(mux);
		pin = (uint32_t)MCHP_XEC_PINMUX_PIN(portpin);
		if (func != MCHP_AF1 && port == 2) {	/* need to disable the BGPO PWR for this pin */

			switch (pin) {
				case 1:
					regs->bgpo_pwr &= ~2;
					break;
				case 2: 
					regs->bgpo_pwr &= ~4;
					break;
				default:
					break;
			}
		} else if (func != MCHP_AF1 && port == 3) {
			switch (pin) {
				case 2:
					regs->bgpo_pwr &= ~8;
					break;
				case 3:
					regs->bgpo_pwr &= ~0x10;
					break;
				case 4:
					regs->bgpo_pwr &= ~0x20;
					break;
				default:
					break;
			}
		}
	}

	return 0;
}

PINCTRL_DT_INST_DEFINE(0);

static struct bgpo_xec_config bgpo_xec_dev_cfg_0 = {
	.regs = (struct bgpo_xec_regs *)(DT_INST_REG_ADDR(0)),
	.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(0),
};

static const struct gpio_driver_api bgpo_xec_api = {
};

DEVICE_DT_INST_DEFINE(0, bgpo_xec_init, NULL,
		      NULL, &bgpo_xec_dev_cfg_0,
		      PRE_KERNEL_1, CONFIG_GPIO_INIT_PRIORITY,
		      &bgpo_xec_api);
