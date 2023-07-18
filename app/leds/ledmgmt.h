/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __LED_MGMT_H__
#define __LED_MGMT_H__

#include "smc.h"

/**
 * @brief LED management task.
 *
 * This routine manages:
 * - Driving the fan
 * - Reading the fan speed through Tach
 * - Reading thermal sensors over ADC
 * - Reading the CPU temperature over PECI or PECI over eSPI channel.
 *
 * @param p1 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 * @param p2 pointer to additional task-specific data.
 *
 */
void ledmgmt_thread(void *p1, void *p2, void *p3);
void ledmgmt_post_thread(void *p1, void *p2, void *p3);

#if 0
/**
 * @brief Host API to update led pwm.
 *
 * This is ACPI hook for host to update led duty cycle for selected led device.
 *
 * @param idx led device index.
 * @param duty_cycle duty cycle.
 */
void host_update_led_dc(uint8_t idx, uint8_t duty_cycle);

void get_led_peripherals_status(uint8_t *hw_peripherals_sts);
#endif

void host_update_led_ownership(uint8_t idx);
void host_update_led_color(uint8_t idx, uint16_t color0, uint16_t color1);
void host_update_led_brightness(uint8_t idx, uint8_t brightness);
void host_update_led_blink(uint8_t idx, uint16_t on, uint16_t off );

#endif	/* __LED_MGMT_H__ */
