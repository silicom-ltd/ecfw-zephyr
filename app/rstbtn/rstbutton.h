/**
 * @file
 * @brief APIs for Reset Button
 */

#ifndef __RESET_BUTTON_H__
#define __RESET_BUTTON_H__

/**
 * @typedef rstbtn_handler_t
 * @brief callback function for modules registered for power button events.
 * @param rstbtn_sts current reset button status.
 */
typedef void (*rstbtn_handler_t)(uint8_t rstbtn_sts);

void rstbutton_init(void);

/**
 * @brief Allows to register modules interested in reset button events.
 *
 */
void rstbtn_register_handler(rstbtn_handler_t handler);

#endif /* __RESET_BUTTON_H__ */
