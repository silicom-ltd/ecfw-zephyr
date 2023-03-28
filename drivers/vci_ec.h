/*
 * Copyright (c) 2022 Silicom Connectivity Solutions Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __VCI_DRIVER_H__
#define __VCI_DRIVER_H__

#include <zephyr/drivers/gpio.h>

/**
 * @brief VCI driver wrapper APIS.
 */
#define EC_VCI_PORT_POS		8U
#define EC_VCI_PORT_MASK	((uint32_t) 0x1U << EC_VCI_PORT_POS)
#define EC_VCI_PIN_POS		0U
#define EC_VCI_PIN_MASK		((uint32_t)0x07U << EC_VCI_PIN_POS)

#define EC_VCI_PORT_PIN(_port, _pin)                   \
	(((uint32_t)(_port) << EC_VCI_PORT_POS)   |       \
	((uint32_t)(_pin) << EC_VCI_PIN_POS))             \

#define HIGH	1
#define LOW	0

/*
 * This structure is used to pass an array of VCIs and settings to
 * this driver wrapper. It encodes the vci device and the pin in a
 * single variable.
 */
struct vci_ec_config {
	uint32_t port_pin;
	uint32_t cfg;
};

static inline uint32_t vci_get_port(uint16_t pin)
{
	return  ((pin & EC_VCI_PORT_MASK) >> EC_VCI_PORT_POS);
}

static inline uint32_t vci_get_pin(uint16_t pin)
{
	return ((pin & EC_VCI_PIN_MASK) >> EC_VCI_PIN_POS);
}

/**
 * @brief Routine to get the absolute vci number
 *
 * This routine gets the absolute gpio number from the port and the
 * pin number.
 *
 * @param p1 a value which combines port and pin values.
 * @retval absolute gpio number.
 */
uint32_t get_absolute_vci_num(uint32_t port_pin);

/**
 * @brief Initialize the vci ports.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_init(void);

/**
 * @brief Configure a specific vci pin.
 *
 * Wrapper interface to configure a vci pin using the standard Zephyr flags.
 * The first parameter hides the port number and pin.
 *
 * @param port_pin Encoded device and pin.
 * @param flags Standard GPIO driver flags to configure a pin.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_configure_pin(uint32_t port_pin, gpio_flags_t flags);

/**
 * @brief Configure an array of vcis.
 *
 * Wrapper to configure several VCIs by receiving
 * an array containing the encoded Port/Pin, flags and direction.
 *
 * @param vcis Array containing device and pin, direction.
 * @param len Array size.
 *
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_configure_array(struct vci_ec_config *vcis, uint32_t len);

/**
 * @brief Read the level of a pin.
 *
 * @param port_pin Encoded port/pin.
 *
 * @retval 1 If pin physical level is high.
 * @retval 0 If pin physical level is low.
 * @retval -ENODEV error when internal device validation failed.
 * @retval Negative errno code on failure.
 */
int vci_read_pin(uint32_t port_pin);

/**
 * @brief Initialize a gpio_callback struct.
 *
 * This wrapper interface initializes a gpio_callback struct.
 * Note: The underlaying zephyr function is void.
 *
 * @param port_pin Encoded port/pin.
 * @param callback A valid Application's callback structure pointer.
 * @param handler A valid handler function pointer.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_init_callback_pin(uint32_t port_pin,
			  struct gpio_callback *callback,
			  gpio_callback_handler_t handler);

/**
 * @brief Add an application callback.
 *
 * This wrapper interface adds a callback pointer to be triggered in the
 * application context.
 *
 * Note: Callbacks may be added to the device from within a callback
 * handler invocation
 *
 * @param port_pin Encoded port/pin.
 * @param callback A valid Application's callback structure pointer.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_add_callback_pin(uint32_t port_pin,
		  	 struct gpio_callback *callback);

/**
 * @brief Remove an application callback.
 *
 * This wrapper interface adds a callback pointer to be triggered in the
 * application context.
 *
 * Note: It is explicitly permitted, within a callback handler, to
 * remove the registration for the callback that is running.
 *
 * @param port_pin Encoded port/pin.
 * @param callback A valid Application's callback structure pointer.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_remove_callback_pin(uint32_t port_pin,
			    struct gpio_callback *callback);

/**
 * @brief Configure pin interrupt.
 *
 * This wrapper interface configures interrupt capabilities for a given pin.
 *
 * @param port_pin Encoded port/pin.
 * @param flags Interrupt configuration flags as defined by GPIO_INT_*.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval 0 if successful, negative errno code on failure.
 */
int vci_interrupt_configure_pin(uint32_t port_pin, gpio_flags_t flags);

/**
 * @brief Validate if a specific gpio port was initialized.
 *
 * This function is expected to be called after gpio_init otherwise
 * it will always return -ENODEV.
 *
 * @param port_pin Encoded port/pin.
 *
 * @retval -ENODEV error when internal device validation failed.
 * @retval true  if successful, false otherwise.
 */
bool vci_port_enabled(uint32_t port_pin);

#endif /* __VCI_DRIVER_H__*/
