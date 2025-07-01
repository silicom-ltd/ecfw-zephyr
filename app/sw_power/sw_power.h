/**
 * @file
 * @brief APIs for Switch power
 */

#ifndef __SW_POWER_SIGNAL_H__
#define __SW_POWER_SIGNAL_H__

/**
 * @typedef sw_power_handler_t
 * @brief callback function for modules registered for switch power events.
 * @param sw_power_sts current switch power status.
 */
typedef void (*sw_power_handler_t)(uint8_t sw_power_sts);

void sw_power_init(void);

/**
 * @brief Allows to register modules interested in swith power events.
 *
 */
void sw_power_register_handler(sw_power_handler_t handler);

#endif /* __SW_POWER_SIGNAL_H__ */
