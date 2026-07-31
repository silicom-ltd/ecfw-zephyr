/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LOM-initiated host power cycle.
 *
 * Runs the sequence the SCM expects:
 *
 *   1. force the host down (power button override)
 *   2. confirm the host is actually down
 *   3. remove switch card power
 *   4. dwell
 *   5. power the host back on
 *   6. switch card power is restored by the power sequencer from power_on()
 *      once ALL_SYS_PWRGD is high, preserving the CPU-then-switch order
 *
 * Step 2 is the point of this module. PLTRST asserting is the *first* signal of
 * the host down sequence, so it says the shutdown has begun, not that it has
 * finished. Restarting on that indication races the remainder of the platform's
 * S0->S5 transition: the power button press can be swallowed by the PCH, and
 * the EC power sequencer can miss the SLP_Sx deassert on the way back up
 * (pwrseq_slp_handler() only accepts it from G3/S3/S4/S5), leaving the host
 * unable to complete power on. This module therefore waits for a positive
 * confirmation that the host is down before touching anything else, and aborts
 * rather than half-cycling if that confirmation never arrives.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/espi.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include "board_config.h"
#include "gpio_ec.h"
#include "pwrplane.h"
#include "task_handler.h"

#include "lom_pwrcycle.h"

LOG_MODULE_REGISTER(lom_pwrcycle, CONFIG_LOM_PWRCYCLE_LOG_LEVEL);

/* Width of a power button press, matching the rest of the EC. */
#define PWRBTN_PULSE_MS   150

/* Granularity of every wait in this module. */
#define POLL_INTERVAL_MS  10

#define LOM_PWRCYCLE_STACK_SIZE 1024

static const struct device *const espi_dev = DEVICE_DT_GET(DT_NODELABEL(espi0));

static K_SEM_DEFINE(pwrcycle_sem, 0, 1);
static atomic_t cycle_active;

/*
 * espi_receive_vwire() is used directly rather than espihub_retrieve_vw()
 * because the latter logs an error whenever the host VW channel is not ready.
 * These helpers poll, so that would flood the console while the host is on its
 * way down.
 */
static bool vw_is_low(enum espi_vwire_signal signal)
{
	uint8_t level;

	if (espi_receive_vwire(espi_dev, signal, &level) != 0) {
		return false;
	}

	return (level == 0);
}

/*
 * True once the PCH has acted on the held power button. PLTRST asserting is the
 * normal indication. The system state is also checked because eSPI reads can
 * start failing as the host drops, and in that case the sequencer leaving S0 is
 * the only evidence available.
 */
static bool host_going_down(void)
{
	if (vw_is_low(ESPI_VWIRE_SIGNAL_PLTRST)) {
		return true;
	}

	return (pwrseq_system_state() != SYSTEM_S0_STATE);
}

/*
 * True once the host is fully down.
 *
 * The sequencer reaches SYSTEM_S5_STATE only after SLP_S3, SLP_S4 and SLP_S5
 * have all asserted and power_off() has completed, which is the strongest
 * confirmation available to the EC. SLP_S5 alone is offered as a fallback for
 * platforms where the forced power off does not assert all three.
 */
static bool host_is_down(void)
{
#ifdef CONFIG_LOM_PWRCYCLE_GATE_SLP_S5
	return vw_is_low(ESPI_VWIRE_SIGNAL_SLP_S5);
#else
	return (pwrseq_system_state() == SYSTEM_S5_STATE);
#endif
}

static int wait_for(bool (*cond)(void), uint32_t timeout_ms)
{
	uint32_t waited = 0;

	while (waited < timeout_ms) {
		if (cond()) {
			return 0;
		}
		k_msleep(POLL_INTERVAL_MS);
		waited += POLL_INTERVAL_MS;
	}

	return cond() ? 0 : -ETIMEDOUT;
}

static void pwrbtn_pulse(void)
{
	gpio_write_pin(PM_PWRBTN, 0);
	k_msleep(PWRBTN_PULSE_MS);
	gpio_write_pin(PM_PWRBTN, 1);
}

/*
 * Hold the power button until the PCH honours the override, then release it.
 *
 * The button must be released as soon as the PCH acts. Holding it across the
 * platform's S5 entry would be seen as a power on request and the host would
 * come back up uncommanded.
 */
static int host_force_off(void)
{
	int ret;

	LOG_DBG("Asserting power button override");

	gpio_write_pin(PM_PWRBTN, 0);
	ret = wait_for(host_going_down, CONFIG_LOM_PWRCYCLE_FORCEOFF_HOLD_MS);
	gpio_write_pin(PM_PWRBTN, 1);

	if (ret) {
		LOG_ERR("Host did not respond to power button override in %d ms",
			CONFIG_LOM_PWRCYCLE_FORCEOFF_HOLD_MS);
		return ret;
	}

	LOG_DBG("Power button released, host is going down");

	return 0;
}

static void pwrcycle_run(void)
{
	LOG_INF("Power cycle requested");

	if (host_force_off()) {
		LOG_ERR("Power cycle aborted, host still running");
		return;
	}

	if (wait_for(host_is_down, CONFIG_LOM_PWRCYCLE_DOWN_WAIT_MS)) {
		LOG_ERR("Power cycle aborted, host not confirmed down in %d ms "
			"(state %d)", CONFIG_LOM_PWRCYCLE_DOWN_WAIT_MS,
			pwrseq_system_state());
		return;
	}

	LOG_INF("Host confirmed down");

#if defined(CONFIG_BOARD_MEC172X_ADL_N_CP)
	switch_card_power_control(0);
#endif

	k_msleep(CONFIG_LOM_PWRCYCLE_DWELL_MS);

	/*
	 * Only the host is powered on here. The power sequencer restores switch
	 * card power from power_on() after ALL_SYS_PWRGD, which is the ordering
	 * the SCM expects.
	 */
	pwrbtn_pulse();

	LOG_INF("Power cycle complete, host power on requested");
}

static void pwrcycle_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (true) {
		k_sem_take(&pwrcycle_sem, K_FOREVER);

		atomic_set(&cycle_active, 1);
		pwrcycle_run();
		atomic_set(&cycle_active, 0);
	}
}

K_THREAD_DEFINE(lom_pwrcycle_tid, LOM_PWRCYCLE_STACK_SIZE, pwrcycle_thread,
		NULL, NULL, NULL, EC_TASK_PRIORITY, 0, 0);

void lom_pwrcycle_start(void)
{
	if (atomic_get(&cycle_active)) {
		LOG_WRN("Power cycle already in progress, request ignored");
		return;
	}

	k_sem_give(&pwrcycle_sem);
}
