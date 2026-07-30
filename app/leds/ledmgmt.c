/*
 * Copyright (c) 2023 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/led.h>
#include <string.h>
#include "ledmgmt.h"
#include "led_mec172x.h"
#include "board_config.h"
#include "board_led.h"
#include "smc.h"
#include "sci.h"
#include "scicodes.h"
#include "smchost.h"
#include "smchost_extended.h"
#include "smchost_commands.h"
#include "pwrplane.h"
#include "memops.h"
#include "gpio_ec.h"
#include "task_handler.h"
//#include "gamma.h"

LOG_MODULE_REGISTER(ledmgmt, CONFIG_LED_MGMT_LOG_LEVEL);

static struct led_dev *led_tbl;
static uint8_t max_led_dev;
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
static struct k_sem led_lock;

void led_request()
{
	k_sem_give(&led_lock);
}
#else
static bool led_update;
#endif

/*
 * Front-panel power LED selection.
 *
 * The front panel has three RGB LEDs stacked vertically (top, middle,
 * bottom). By default the top RGB LED (pwmmcled0, PWM4/5/6) is the power LED
 * and carries the breathing power-state indication. The Netgate SKU instead
 * uses the bottom RGB LED (pwmmcled2, PWM2/9/3). Everything downstream drives
 * whichever node landed in power_led.
 *
 * The SKU comes from the ONIE "TlvInfo" FRU. The board caches the whole FRU in
 * board_init() as the single source of truth; we read that cache through
 * board_fru_data() rather than doing our own I2C read.
 *
 * Panel layout is confirmed from the Netgate front panel: on Netgate the power
 * LED is the bottom of the three; on standard Ibiza it is the top.
 *
 * The DT PWM channels are pwmmcled0 -> PWM4/5/6, pwmmcled1 -> PWM10/7/8,
 * pwmmcled2 -> PWM2/9/3. We assume the node index runs top->bottom
 * (pwmmcled0 = top, pwmmcled2 = bottom); this node<->position wiring has not
 * yet been bench-verified against the Ibiza/Netgate front-panel schematic. If
 * pwmmcled2 is not the physical bottom LED, only POWER_LED_NETGATE (and
 * POWER_LED_DEFAULT) need to change.
 */
#define POWER_LED_DEFAULT	DT_NODELABEL(pwmmcled0)
#define POWER_LED_NETGATE	DT_NODELABEL(pwmmcled2)

static const struct device *power_led;
static uint8_t power_led_idx;	/* led_tbl index of power_led (for host handoff) */
static uint8_t host_power_idx;	/* led_tbl index the host uses for the power LED */

/* ONIE "TlvInfo" FRU header: "TlvInfo\0" magic + version + 2-byte length. */
#define FRU_HDR_SIZE		11
#define FRU_MFG_NETGATE		"Netgate"

/*
 * Weak fallback: boards that cache the FRU in board_init() provide a strong
 * board_fru_data(); everything else reports "no FRU" so LED selection keeps
 * legacy (top LED) behavior.
 */
__attribute__((weak)) const uint8_t *board_fru_data(uint16_t *len)
{
	if (len) {
		*len = 0;
	}
	return NULL;
}

static inline char fru_lc(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

/* Case-insensitive search for `needle` anywhere in [buf, buf+len). */
static bool fru_mem_contains_ci(const uint8_t *buf, uint16_t len, const char *needle)
{
	size_t nlen = strlen(needle);

	if (nlen == 0 || len < nlen) {
		return false;
	}
	for (uint16_t i = 0; i + nlen <= len; i++) {
		size_t j = 0;

		while (j < nlen && fru_lc((char)buf[i + j]) == fru_lc(needle[j])) {
			j++;
		}
		if (j == nlen) {
			return true;
		}
	}
	return false;
}

/*
 * Return true if the cached FRU identifies a Netgate SKU.
 *
 * The Netgate marker is present in the FRU (confirmed on target), but the exact
 * Silicom custom TLV that carries it (sys-manufacturer 0x51 vs sys-SKU 0x56 vs
 * ...) is not yet pinned down, and it may live in the second EEPROM page
 * (0x57). So we search the whole combined image for the marker string rather
 * than a single TLV field -- robust regardless of which field holds it. The
 * board's cache_fru() hex dump lets us tighten this to a specific TLV later.
 *
 * On any missing/invalid FRU we fall back to false (default top LED).
 */
static bool fru_is_netgate(void)
{
	static const uint8_t fru_hdr_title[] = "TlvInfo";
	uint16_t fru_len = 0;
	const uint8_t *fru = board_fru_data(&fru_len);

	if (fru == NULL || fru_len < FRU_HDR_SIZE) {
		LOG_WRN("FRU unavailable; using default power LED");
		return false;
	}

	if (memcmp(fru, fru_hdr_title, 8)) {
		LOG_WRN("Unsupported FRU format; using default power LED");
		return false;
	}

	bool netgate = fru_mem_contains_ci(fru, fru_len, FRU_MFG_NETGATE);

	LOG_INF("FRU Netgate marker %sfound", netgate ? "" : "NOT ");
	return netgate;
}

/*
 * Resolve which RGB node is the power LED and map it to its led_tbl index, so
 * the OS-handoff gate in manage_local_leds() watches the same physical LED the
 * EC breathes on.
 *
 * host_power_idx is the index the host uses to mean "the power LED": ACPI is
 * SKU-agnostic and always addresses the default (top) node, so on Netgate the
 * host index and the physical index differ and led_host_to_phys() bridges them.
 *
 * Runs once at task startup, after init_leds() has populated led_tbl.
 */
static void select_power_led(void)
{
	const struct device *default_led = DEVICE_DT_GET(POWER_LED_DEFAULT);

	if (fru_is_netgate()) {
		power_led = DEVICE_DT_GET(POWER_LED_NETGATE);
		LOG_INF("Netgate SKU: power LED = bottom (pwmmcled2)");
	} else {
		power_led = default_led;
		LOG_INF("Default SKU: power LED = top (pwmmcled0)");
	}

	power_led_idx = 0;
	host_power_idx = 0;
	for (uint8_t i = 0; i < max_led_dev; i++) {
		if (led_tbl[i].dev == power_led) {
			power_led_idx = i;
		}
		if (led_tbl[i].dev == default_led) {
			host_power_idx = i;
		}
	}
	LOG_INF("power LED idx = %u (host addresses %u)", power_led_idx,
		host_power_idx);
}

/*
 * Translate a host-supplied LED index into a led_tbl index.
 *
 * The host (ACPI) does not know about the SKU: it always drives the power LED
 * at the default index, so on Netgate its writes -- e.g. the solid blue it sets
 * once the OS is up -- would land on the top LED while the EC is still
 * breathing on the bottom one, and the ownership gate would never trip.
 *
 * Swapping rather than one-way mapping keeps the other RGB LED addressable: the
 * host index the power LED vacated now reaches the node the power LED isn't
 * using. On the standard SKU the two indices are equal and this is a no-op.
 *
 * Note this assumes the host addresses the power LED at host_power_idx on both
 * SKUs. If the OS is instead SKU-aware and already sends the physical index,
 * this swap would undo that -- bench-verify against the ACPI/OS side before
 * shipping (see also the FRU-marker caveat in fru_is_netgate()).
 */
static uint8_t led_host_to_phys(uint8_t idx)
{
	if (idx >= max_led_dev || power_led_idx == host_power_idx) {
		return idx;
	}
	if (idx == host_power_idx) {
		return power_led_idx;
	}
	if (idx == power_led_idx) {
		return host_power_idx;
	}
	return idx;
}

#if 0
struct led_device *led_dev_tbl;
static uint8_t max_led_dev;
static uint8_t led_duty_cycle[16];
static bool led_duty_cycle_change;

void get_pwm_led_peripherals_status(uint8_t *hw_peripherals_sts)
{
	uint8_t idx = 0x0;
#if 0
	/* First byte contains led status, bits 0-3 are led index, bit 7 is
	 * led presence, 1 = present, 0 = not present.  2nd byte indicates type 
	 * (bits 3-0, bits 7-4 indicate group if RGB).
	 * 3rd byte indicates color (if 2nd byte is RGB), 4th byte indicates
	 * panel and position (bits 0-2 panel, 3-4 vertical, 5-6 horizontal).
	 * Panel:
	 * 	0 - Top
	 * 	1 - Bottom
	 * 	2 - Left
	 * 	3 - Right
	 * 	4 - Front
	 * 	5 - Back
	 * 	6 - Unknown
	 *
	 * Vertical Position:
	 * 	0 - Upper
	 * 	1 - Center
	 * 	2 - Lower
	 *
	 * Horzontal Position:
	 * 	0 - Left
	 * 	1 - Center
	 * 	2 - Right 
	 */
#endif
	/*
	 * 2 bytes contain bitmap of led pwms
	 */

	/* Update pwm led status */
	for (idx = 0; idx < max_pwm_dev; idx++) {
		if (led_dev_tbl[idx].num.pwm < 8)
			hw_peripherals_sts[0] |= BIT(led_dev_tbl[idx].num.pwm);
		else
			hw_peripherals_sts[1] |= BIT(led_dev_tbl[idx].num.pwm - 8);
	}
}
#endif

static void init_leds(void)
{
	board_led_dev_tbl_init(&max_led_dev, &led_tbl);
	LOG_INF("Found %d leds", max_led_dev);
	led_init(max_led_dev, led_tbl);

}

/* Takes a led_tbl (physical) index; host indices go through led_host_to_phys. */
bool is_led_controlled_by_host(uint8_t idx)
{
	if (idx >= max_led_dev) {
		return 0;
	}

	if (!is_system_in_acpi_mode()) {
		LOG_DBG("LED control is over-ridden when not in ACPI mode.");
		return 0;
	}

	if (!led_tbl[idx].owned) {
		LOG_INF("LED %d is not owned by host", idx);
		return 0;
	}
	return 1;
}

void host_update_led_ownership(uint8_t idx)
{
	idx = led_host_to_phys(idx);

	if (idx < max_led_dev) {
		led_tbl[idx].owned = 1;
	}
}

/*
 * Drop all host LED ownership, handing the LEDs back to the EC.
 *
 * Ownership is otherwise set-only: nothing clears it on reset, and the OS sends
 * no DISABLE_ACPI on reboot or poweroff, so without this the very first
 * SLCM.RSET() would own the power LED for the rest of the EC's uptime. Every
 * later boot would then skip the EC's boot indication and sit on whatever color
 * was last programmed.
 *
 * Called on PLTRST# assertion, which covers both a warm reboot and the reset
 * that precedes any cold boot, so the boot indication runs on every host boot.
 */
void host_clear_all_led_ownership(void)
{
	for (uint8_t i = 0; i < max_led_dev; i++) {
		led_tbl[i].owned = 0;
	}
}

void host_update_led_color(uint8_t idx, uint16_t greenblue, uint16_t red)
{
	idx = led_host_to_phys(idx);

	if (!is_led_controlled_by_host(idx)) {
		LOG_INF("LED %d is not in host control", idx);
		return;
	}
	if (!led_tbl[idx].rgb) {
		LOG_INF("LED %d is not an RGB led", idx);
		return;
	}
	if (idx < max_led_dev) {
		led_tbl[idx].color = ((red & 0xFF) << 16) | greenblue;
		led_tbl[idx].update_color = 1;
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		led_update = 1;
#endif
		LOG_INF("Updating led %d color", idx);
	} else {
		LOG_WRN("Invalid led idx %d", idx);
	}
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	led_request();
#endif
}

void host_update_led_brightness(uint8_t idx, uint8_t brightness)
{
	idx = led_host_to_phys(idx);

	if (!is_led_controlled_by_host(idx)) {
		LOG_INF("LED %d is not in host control", idx);
		return;
	}

	if (idx < max_led_dev) {
		led_tbl[idx].brightness = brightness;
		led_tbl[idx].update_brightness = 1;
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		led_update = 1;
#endif
		LOG_INF("Updating led %d brightness", idx);
	} else {
		LOG_WRN("Invalid led idx %d", idx);
	}
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	led_request();
#endif
}

void host_update_led_blink(uint8_t idx, uint16_t on, uint16_t off)
{
	idx = led_host_to_phys(idx);

	if (!is_led_controlled_by_host(idx)) {
		LOG_INF("LED %d is not in host control", idx);
		return;
	}
	if (!led_tbl[idx].rgb && !led_tbl[idx].pwm) {
		LOG_INF("LED %d is not PWM-controlled", idx);
		return;
	}
	if (idx < max_led_dev) {
		led_tbl[idx].on = on;
		led_tbl[idx].off = off;
		led_tbl[idx].update_blink = 1;
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		led_update = 1;
#endif
		LOG_INF("Updating led %d blink rate", idx);
	} else {
		LOG_WRN("Invalid led idx %d", idx);
	}
#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	led_request();
#endif
}

static void manage_leds(void)
{
	/* 
	 * ignore any attempt to control leds outside of S0
	 */
	if ((pwrseq_system_state() != SYSTEM_S0_STATE) ||
		(smchost_is_system_in_cs())) {
		return;
	}
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	if (led_update) {
		led_update = 0;
#endif
		for (uint8_t idx = 0; idx < max_led_dev; idx++) {
			if (led_tbl[idx].update_color) {
				uint8_t color[3];
				color[0] = (led_tbl[idx].color) >> 16;
				color[1] = ((led_tbl[idx].color) >> 8) & 0xFF;
				color[2] = led_tbl[idx].color & 0xFF;
				led_color_set(idx, color);
				led_tbl[idx].update_color = 0;
			}	
#if 0
			if (led_tbl[idx].update_brightness) {
				led_brightness_set(idx, led_tbl[idx].brightness);
				led_tbl[idx].update_brightness = 0;
			}
#endif
			if (led_tbl[idx].update_blink) {
				led_blink_set(idx, led_tbl[idx].on, led_tbl[idx].off);
				led_tbl[idx].update_blink = 0;
			}
		}
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	}
#endif
#if 1
	for (uint8_t idx = 0; idx < max_led_dev; idx++) {
		if (!led_tbl[idx].on || led_tbl[idx].update_brightness) { // don't change brightness of a blinker
			led_brightness_set(idx, led_tbl[idx].brightness);
			led_tbl[idx].update_brightness = 0;
			if (led_tbl[idx].on) {
				led_tbl[idx].on = 0;
				led_tbl[idx].update_blink = 1;
			}
		}
	}

#endif
}

static int color_table[] = {
#if 0
	0x9D5FB0, /* magenta-ish */
	0x318261, /* dull green */
	0x0066CC, /* a nice blue */
	0xFF66FF, /* mid pink */
	0xFFFF00, /* strong yellow */
	0x99FF99, /* very light green */
	0xF0183C, /* darkish red */
#endif
	0xFF7F00, /* amber */
	0x00FF00, /* green */
};

#define COLOR_AMBER	0
#define COLOR_GREEN	1

/*
 * The power LED breathes in both EC-owned states; only the color differs.
 * Amber until the platform reaches S0, then green from S0 until the OS claims
 * the LED -- so the whole SBL/UEFI boot window is breathing green. Once the
 * host owns the LED the EC stops driving it.
 *
 * The ramp advances one step per task tick (led_thrd_period, 5 ms), so a full
 * breath is ~1 s. Step `level` by more than 1, or only every Nth tick, to slow
 * it down.
 */
static void manage_local_leds(void)
{
	static uint16_t level = 0;
	static uint16_t countup = 1;
	static bool handed_off;
	const struct device *led_pwm_mc = power_led;
	int err;
	int color_choice;
	bool in_s0;
	uint8_t colors[3];

	/* power_led is resolved before the task loop starts; guard anyway. */
	if (led_pwm_mc == NULL) {
		return;
	}

	in_s0 = (pwrseq_system_state() == SYSTEM_S0_STATE);

	if (in_s0 && is_led_controlled_by_host(power_led_idx)) {
		/*
		 * Host owns the LED now. Blank the breathe once on the way out,
		 * otherwise it stays frozen at whatever level the ramp had
		 * reached; manage_leds() then applies the host's own color and
		 * brightness. Re-armed below if control ever comes back.
		 */
		if (!handed_off) {
			led_set_brightness(led_pwm_mc, 0, 0);
			level = 0;
			countup = 1;
			handed_off = true;
			LOG_INF("power LED handed off to host");
		}
		return;
	}
	handed_off = false;

	color_choice = in_s0 ? COLOR_GREEN : COLOR_AMBER;

	colors[0] = color_table[color_choice] >> 16;
	colors[1] = (color_table[color_choice] >> 8) & 0xFF;
	colors[2] = color_table[color_choice] & 0xFF;

	led_set_color(led_pwm_mc, 0, 3, colors);
	err = led_set_brightness(led_pwm_mc, 0, level);
	if (err)
		return;

	if (countup)
		level++;
	else
		level--;

	if (level == 100)
		countup = 0;
	else if (level == 0)
		countup = 1;
}

void ledmgmt_thread(void *p1, void *p2, void *p3)
{
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	uint32_t normal_period = *(uint32_t *)p1;
#endif

	LOG_INF("LEDMGMT thread starting");
	init_leds();

	/*
	 * Requirement order: all LEDs are already off after init_leds(); now
	 * read the FRU to pick the power LED before any breathing starts.
	 */
	select_power_led();

#ifdef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
	k_sem_init(&led_lock, 0, 1);
#endif
	while (true) {
#ifndef CONFIG_SMCHOST_EVENT_DRIVEN_TASK
		k_msleep(normal_period);
		manage_local_leds();
		manage_leds();
#else
		manage_local_leds();
		k_sem_take(&led_lock, Z_TIMEOUT_MS(5));
		manage_leds();
#endif
	}
}

#ifdef CONFIG_LED_MANAGEMENT_POST
void ledmgmt_post_thread(void *p1, void *p2, void *p3)
{
	extern struct acpi_tbl g_acpi_tbl;

	uint32_t normal_period = *(uint32_t *)p1;
	static int color_choice = 0;
	static int blinking = 0;

	LOG_INF("LEDMGMT_POST thread starting");

	while (true) {
#if 0
		if (!blinking) {
			k_msleep(normal_period);
			g_acpi_tbl.acpi_led_val_l = 0;
			g_acpi_tbl.acpi_led_val_h = 0;
			smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BLINK);
		} else {
			k_msleep(normal_period*4);
			blinking++;
			blinking %= 4;
		}
#endif
		if (!blinking) {
			k_msleep(normal_period);
		if (pwrseq_system_state() != SYSTEM_S0_STATE) {
			g_acpi_tbl.acpi_led_val_l = 0;
			g_acpi_tbl.acpi_led_val_h = 0;
			smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BRIGHTNESS);
			continue;
		}
		g_acpi_tbl.acpi_led_val_l = color_table[color_choice] & 0xFFFF;
		g_acpi_tbl.acpi_led_val_h = color_table[color_choice] >> 16;
		g_acpi_tbl.acpi_led_idx = 0;

		smchost_cmd_led_handler(SMCHOST_UPDATE_LED_COLOR);
		g_acpi_tbl.acpi_led_val_l = 50;
		smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BRIGHTNESS);
		color_choice++;
		if (color_choice == 7) {
			g_acpi_tbl.acpi_led_val_l = 100;
			g_acpi_tbl.acpi_led_val_h = 100;
			smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BLINK);
			blinking = 1;
		}
		color_choice %= ARRAY_SIZE(color_table);
		} else {
			k_msleep(normal_period*4);
			blinking++;
			blinking %= 4;
			if (!blinking) {
				g_acpi_tbl.acpi_led_val_l = 1;
				g_acpi_tbl.acpi_led_val_h = 0;
				smchost_cmd_led_handler(SMCHOST_UPDATE_LED_BLINK);
			}
		}
	}
}
#endif
