/**
 * @file
 * @brief APIs for Location Button
 */

#ifndef __LOCATION_BUTTON_H__
#define __LOCATION_BUTTON_H__

/**
 * @typedef btn_handler_t
 * @brief callback function for modules registered for button events.
 * @param btn_sts current button status.
 */
typedef void (*btn_handler_t)(uint8_t btn_sts);

void location_button_init(void);

/**
 * @brief Allows to register modules interested in location button events.
 *
 */
void btn_register_handler(btn_handler_t handler);

#endif /* __LOCATION_BUTTON_H__ */
