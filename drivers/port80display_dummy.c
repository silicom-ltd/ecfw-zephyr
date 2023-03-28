/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/logging/log.h>
#include "port80display.h"
LOG_MODULE_REGISTER(port80, CONFIG_POSTCODE_LOG_LEVEL);

/* Scan Display digits 0-3 */
#define PORT80_DISPLAY_DIGITS  3

int port80_display_init(void)
{
	return 0;
}

int port80_display_on(void)
{
	return 0;
}

int port80_display_off(void)
{
	return 0;
}

void port80_display_word(uint16_t word)
{
	return;
}
