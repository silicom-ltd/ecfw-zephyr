#ifndef _VCI_DRIVER_H_
#define _VCI_DRIVER_H_

#include <zephyr/drivers/gpio.h>

int vci_init(void);

int vci_init_callback_pin(uint32_t pin,
			  struct gpio_callback *callback,
			  gpio_callback_handler_t handler);

int vci_add_callback_pin(uint32_t pin,
			 struct gpio_callback *callback);

#endif /* _VCI_DRIVER_H_ */
