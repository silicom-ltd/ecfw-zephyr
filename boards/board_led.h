/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BOARD_LED_H__
#define __BOARD_LED_H__


/**
 * @brief Get the pointer to led list table defined for the board.
 *
 * @param p_max_led pointer to update led table size.
 * @param p_led_tbl pointer to led list table.
 */
void board_led_dev_tbl_init(uint8_t *pmax_led, struct led_dev **pled_tbl);

#endif /* __BOARD_LED_H__ */

