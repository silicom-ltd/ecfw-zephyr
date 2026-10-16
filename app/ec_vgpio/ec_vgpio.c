/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "acpi_region.h"
#include "sci.h"
#include "scicodes.h"
#include "ec_vgpio.h"
#include "ec_vgpio_region.h"

LOG_MODULE_REGISTER(ec_vgpio, CONFIG_EC_VGPIO_LOG_LEVEL);

/* The board's silicom,ec-vgpio node - one child per exposed line, in the
 * order that fixes each line's EC ID (0-based).
 */
#define EC_VGPIO_NODE DT_COMPAT_GET_ANY_STATUS_OKAY(silicom_ec_vgpio)

BUILD_ASSERT(DT_NODE_EXISTS(EC_VGPIO_NODE),
	     "CONFIG_EC_VGPIO requires a 'silicom,ec-vgpio' devicetree node");

BUILD_ASSERT(sizeof(struct ec_vgpio_region) <=
	     sizeof(((struct acpi_tbl *)0)->ec_vgpio_buffer),
	     "struct ec_vgpio_region must fit within acpi_tbl's ec_vgpio_buffer[33]");

/* Overlaid directly onto app/smchost's g_acpi_tbl.ec_vgpio_buffer - the host
 * addresses this region through the same indexed ACPI_EC_0 reads/writes
 * (and gets the same automatic generate_sci() on each one) as every other
 * field in g_acpi_tbl.
 */
#define g_vgpio_region (*(struct ec_vgpio_region *)(g_acpi_tbl.ec_vgpio_buffer))

struct ec_vgpio_line {
	struct gpio_dt_spec gpio;
	const char *name;
	uint8_t flags;		/* EC_VGPIO_FL_* capability flags, fixed at init */
	uint8_t irq_cfg;	/* desired IRQ config, set by the host */
	uint8_t irq_cfg_hw;	/* last IRQ config actually applied to hardware */
	struct gpio_callback cb;
};

#define EC_VGPIO_LINE_FLAGS(node)					\
	((DT_PROP(node, input) ? EC_VGPIO_FL_INPUT : 0) |		\
	 (DT_PROP(node, output) ? EC_VGPIO_FL_OUTPUT : 0) |		\
	 (DT_PROP(node, host_settable) ? EC_VGPIO_FL_HOST_DRIVE : 0) |	\
	 ((DT_GPIO_FLAGS(node, gpios) & GPIO_ACTIVE_LOW) ?		\
	  EC_VGPIO_FL_ACTIVE_LOW : 0) |				\
	 (DT_PROP(node, irq_capable) ? EC_VGPIO_FL_IRQ : 0))

#define EC_VGPIO_LINE_INIT(node)					\
	{								\
		.gpio = GPIO_DT_SPEC_GET(node, gpios),			\
		.name = DT_PROP_OR(node, label, DT_NODE_FULL_NAME(node)), \
		.flags = EC_VGPIO_LINE_FLAGS(node),			\
	},

static struct ec_vgpio_line lines[] = {
	DT_FOREACH_CHILD_STATUS_OKAY(EC_VGPIO_NODE, EC_VGPIO_LINE_INIT)
};

#define EC_VGPIO_NUM_LINES ARRAY_SIZE(lines)

BUILD_ASSERT(EC_VGPIO_NUM_LINES > 0,
	     "'silicom,ec-vgpio' node has no child lines");
BUILD_ASSERT(EC_VGPIO_NUM_LINES <= EC_VGPIO_MAX_LINES,
	     "too many ec-vgpio lines for an 8-bit line ID - raise "
	     "EC_VGPIO_MAX_LINES or trim the devicetree");

/* Pending GPIO edge events, drained by the host via mailbox command 6. */
struct ec_vgpio_event {
	uint8_t id;
	uint8_t flags;
};

static struct ec_vgpio_event evq[CONFIG_EC_VGPIO_EVQ_DEPTH];
static uint8_t evq_head;
static uint8_t evq_count;
static bool evq_overflow;
static struct k_spinlock evq_lock;

static struct k_work notify_work;

static void notify_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	enqueue_sci(SCI_EC_VGPIO_EVENT);
	generate_sci();
}

/* Called from GPIO ISR context: keep this minimal (push + defer). */
static void evq_push(uint8_t id, int level)
{
	k_spinlock_key_t key = k_spin_lock(&evq_lock);
	bool was_empty = (evq_count == 0);
	uint8_t tail;

	if (evq_count == ARRAY_SIZE(evq)) {
		evq_overflow = true;
		evq_head = (evq_head + 1) % ARRAY_SIZE(evq);
		evq_count--;
	}

	tail = (evq_head + evq_count) % ARRAY_SIZE(evq);
	evq[tail].id = id;
	evq[tail].flags = level ? EC_VGPIO_EV_LEVEL : 0;
	evq_count++;

	k_spin_unlock(&evq_lock, key);

	if (was_empty) {
		k_work_submit(&notify_work);
	}
}

static bool evq_pop(struct ec_vgpio_event *out)
{
	k_spinlock_key_t key = k_spin_lock(&evq_lock);
	bool got = false;

	if (evq_count > 0) {
		*out = evq[evq_head];
		if (evq_overflow) {
			out->flags |= EC_VGPIO_EV_OVERFLOW;
			evq_overflow = false;
		}
		evq_head = (evq_head + 1) % ARRAY_SIZE(evq);
		evq_count--;
		got = true;
	}

	k_spin_unlock(&evq_lock, key);
	return got;
}

static bool id_valid(uint8_t id)
{
	return id < EC_VGPIO_NUM_LINES;
}

static bool writable(const struct ec_vgpio_line *l)
{
	return (l->flags & (EC_VGPIO_FL_OUTPUT | EC_VGPIO_FL_HOST_DRIVE)) ==
	       (EC_VGPIO_FL_OUTPUT | EC_VGPIO_FL_HOST_DRIVE);
}

static void apply_irq_cfg(struct ec_vgpio_line *l)
{
	gpio_flags_t flags = GPIO_INT_DISABLE;

	if (l->irq_cfg & EC_VGPIO_IRQ_EN) {
		switch (l->irq_cfg & EC_VGPIO_IRQ_TRIG_MASK) {
		case EC_VGPIO_IRQ_RISING:
			flags = GPIO_INT_EDGE_RISING;
			break;
		case EC_VGPIO_IRQ_FALLING:
			flags = GPIO_INT_EDGE_FALLING;
			break;
		case EC_VGPIO_IRQ_BOTH:
			flags = GPIO_INT_EDGE_BOTH;
			break;
		default:
			/* enabled with no trigger selected: treat as disabled */
			flags = GPIO_INT_DISABLE;
			break;
		}
	}

	if (gpio_pin_interrupt_configure_dt(&l->gpio, flags)) {
		LOG_WRN("line %u: IRQ config 0x%02x failed", (unsigned int)(l - lines),
			l->irq_cfg);
		return;
	}

	l->irq_cfg_hw = l->irq_cfg;
}

static void ec_vgpio_gpio_isr(const struct device *dev, struct gpio_callback *cb,
			      uint32_t pins)
{
	struct ec_vgpio_line *l = CONTAINER_OF(cb, struct ec_vgpio_line, cb);
	int level = gpio_pin_get_dt(&l->gpio);

	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	evq_push((uint8_t)(l - lines), level);
}

/* ---- mailbox command handlers -------------------------------------------- */

static void cmd_set(void)
{
	uint8_t id = g_vgpio_region.vgid;
	uint8_t val = g_vgpio_region.vgvl;

	if (!id_valid(id)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_BAD_ID;
		return;
	}
	if (val > 1) {
		g_vgpio_region.vgst = EC_VGPIO_ST_BAD_VALUE;
		return;
	}
	if (!writable(&lines[id])) {
		g_vgpio_region.vgst = EC_VGPIO_ST_DENIED;
		return;
	}

	if (gpio_pin_set_dt(&lines[id].gpio, val)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_DENIED;
		return;
	}

	g_vgpio_region.vgst = EC_VGPIO_ST_OK;
}

static void cmd_get(void)
{
	uint8_t id = g_vgpio_region.vgid;
	int val;

	if (!id_valid(id)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_BAD_ID;
		return;
	}

	val = gpio_pin_get_dt(&lines[id].gpio);
	if (val < 0) {
		g_vgpio_region.vgst = EC_VGPIO_ST_DENIED;
		return;
	}

	g_vgpio_region.vgvl = (uint8_t)val;
	g_vgpio_region.vgst = EC_VGPIO_ST_OK;
}

static void cmd_line_count(void)
{
	g_vgpio_region.vgvl = EC_VGPIO_NUM_LINES;
	g_vgpio_region.vgst = EC_VGPIO_ST_OK;
}

static void cmd_describe(void)
{
	uint8_t idx = g_vgpio_region.vgid;
	const struct ec_vgpio_line *l;

	if (!id_valid(idx)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_BAD_ID;
		return;
	}

	l = &lines[idx];
	g_vgpio_region.vgvl = idx;
	g_vgpio_region.vgfl = l->flags;

	memset(g_vgpio_region.vgnm, 0, sizeof(g_vgpio_region.vgnm));
	if (l->name) {
		strncpy((char *)g_vgpio_region.vgnm, l->name,
			sizeof(g_vgpio_region.vgnm) - 1);
	}

	g_vgpio_region.vgst = EC_VGPIO_ST_OK;
}

static void cmd_irq_cfg(void)
{
	uint8_t id = g_vgpio_region.vgid;
	uint8_t cfg = g_vgpio_region.vgvl;

	if (!id_valid(id)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_BAD_ID;
		return;
	}
	if (!(lines[id].flags & EC_VGPIO_FL_IRQ)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_DENIED;
		return;
	}

	lines[id].irq_cfg = cfg;
	apply_irq_cfg(&lines[id]);
	g_vgpio_region.vgst = EC_VGPIO_ST_OK;
}

static void cmd_pop_event(void)
{
	struct ec_vgpio_event ev;

	if (!evq_pop(&ev)) {
		g_vgpio_region.vgst = EC_VGPIO_ST_QUEUE_EMPTY;
		return;
	}

	g_vgpio_region.vgvl = ev.id;
	g_vgpio_region.vgfl = ev.flags;
	g_vgpio_region.vgst = EC_VGPIO_ST_OK;
}

void ec_vgpio_handle_ec_write(uint8_t acpi_idx)
{
	if (acpi_idx != offsetof(struct acpi_tbl, ec_vgpio_buffer) +
			offsetof(struct ec_vgpio_region, vgcm)) {
		return;
	}

	switch (g_vgpio_region.vgcm) {
	case EC_VGPIO_CMD_SET:
		cmd_set();
		break;
	case EC_VGPIO_CMD_GET:
		cmd_get();
		break;
	case EC_VGPIO_CMD_LINE_COUNT:
		cmd_line_count();
		break;
	case EC_VGPIO_CMD_DESCRIBE:
		cmd_describe();
		break;
	case EC_VGPIO_CMD_IRQ_CFG:
		cmd_irq_cfg();
		break;
	case EC_VGPIO_CMD_POP_EVENT:
		cmd_pop_event();
		break;
	default:
		LOG_WRN("unsupported command: %02x", g_vgpio_region.vgcm);
		g_vgpio_region.vgst = EC_VGPIO_ST_BAD_VALUE;
		break;
	}

	/* EC clears the command once it has completed. */
	g_vgpio_region.vgcm = 0;
}

void ec_vgpio_init(void)
{
	k_work_init(&notify_work, notify_work_handler);

	for (unsigned int i = 0; i < EC_VGPIO_NUM_LINES; i++) {
		struct ec_vgpio_line *l = &lines[i];

		if (!device_is_ready(l->gpio.port)) {
			LOG_ERR("line %u (%s): GPIO port not ready", i, l->name);
			continue;
		}

		gpio_pin_configure_dt(&l->gpio, (l->flags & EC_VGPIO_FL_OUTPUT) ?
				      GPIO_OUTPUT_INACTIVE : GPIO_INPUT);

		if (!(l->flags & EC_VGPIO_FL_IRQ)) {
			continue;
		}

		l->irq_cfg = 0;
		l->irq_cfg_hw = 0;
		gpio_pin_interrupt_configure_dt(&l->gpio, GPIO_INT_DISABLE);

		gpio_init_callback(&l->cb, ec_vgpio_gpio_isr, BIT(l->gpio.pin));
		if (gpio_add_callback(l->gpio.port, &l->cb)) {
			LOG_ERR("line %u (%s): failed to add GPIO callback", i, l->name);
		}
	}

	evq_head = 0;
	evq_count = 0;
	evq_overflow = false;

	LOG_INF("%u EC virtual GPIO lines registered", EC_VGPIO_NUM_LINES);
}
