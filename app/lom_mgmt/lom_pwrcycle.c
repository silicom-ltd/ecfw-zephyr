/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * LOM-initiated host power cycle.
 *
 * Implements the sequence the SCM expects, with an explicit completion
 * criterion for every step:
 *
 *   step                          completion criterion
 *   ----------------------------  --------------------------------------------
 *   1 LOM sends power cycle cmd   state == SYSTEM_S0_STATE, no cycle in flight
 *                                 (validated by the caller)
 *   2 Gaia starts shutdown        PLTRST asserts
 *   3 Shutdown complete           state == SYSTEM_S5_STATE
 *   4 Switch shutdown             SW_PWR_OK low
 *   5 Wait a few seconds          elapsed time, measured from step 4
 *   6 Switch power on             SW_PWR_OK high
 *   7 CPU power on                state == SYSTEM_S0_STATE
 *
 * Step 2 is a FORCED power off. PM_PWRBTN is asserted and held until the PCH
 * honours its power button override; there is no ACPI shutdown request and no
 * graceful wait. This replaces the previous behaviour, which pressed the button
 * for 150 ms as an ACPI request, waited up to
 * CONFIG_HOST_GRACEFUL_SHUTDOWN_WAIT_TIME (30 s) for the OS to go down on its
 * own, and only then held the button to force it -- up to ~37 s before the
 * cycle could continue. A LOM power cycle is not expected to be graceful;
 * PWC_SHUTDOWN remains available for that.
 *
 * Step 3 is the point of this module. PLTRST asserting is the *first* signal of
 * the down sequence, so it ends step 2 but says nothing about step 3. Using it
 * for both -- which is what the previous implementation did -- starts the step 5
 * dwell too early and presses the power button while the platform may still be
 * mid-transition. The press can then be swallowed by the PCH, and the EC
 * sequencer can reject the SLP_Sx deassert on the way back up
 * (pwrseq_slp_handler() accepts it only from G3/S3/S4/S5), leaving power_on()
 * unrun and the host unable to complete power up.
 *
 * No single hardware signal means "shutdown complete", so step 3 uses the
 * sequencer state, which is set only after SLP_S3, SLP_S4 and SLP_S5 have all
 * asserted and power_off() has returned. It also rejects a warm reboot, which
 * never completes the SLP mask.
 *
 * Every wait is bounded and a failure abandons the cycle rather than
 * half-cycling the box.
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
 * Step 2. True once the PCH has acted on the held power button. PLTRST
 * asserting is the normal indication. The system state is also checked because
 * eSPI reads can start failing as the host drops, and in that case the
 * sequencer leaving S0 is the only evidence available.
 */
static bool host_going_down(void)
{
	if (vw_is_low(ESPI_VWIRE_SIGNAL_PLTRST)) {
		return true;
	}

	return (pwrseq_system_state() != SYSTEM_S0_STATE);
}

/*
 * Step 3. True once the host is fully down.
 *
 * SLP_S5 alone is offered as a fallback for platforms where the forced power
 * off does not assert all three SLP_Sx and the sequencer therefore never
 * reaches SYSTEM_S5_STATE. It is weaker: SLP_S5 asserts before power_off()
 * completes.
 */
static bool host_is_down(void)
{
#ifdef CONFIG_LOM_PWRCYCLE_GATE_SLP_S5
	return vw_is_low(ESPI_VWIRE_SIGNAL_SLP_S5);
#else
	return (pwrseq_system_state() == SYSTEM_S5_STATE);
#endif
}

/* Step 7. True once the sequencer has completed its own power up. */
static bool host_is_up(void)
{
	return (pwrseq_system_state() == SYSTEM_S0_STATE);
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
 * Steps 4 and 6. switch_card_power_control() drives SW_PWR_ON_OFF and blocks
 * until SW_PWR_OK follows, so the dwell that follows step 4 is measured from a
 * confirmed power down rather than from a request.
 *
 * It returns void and only logs on failure, so a stuck SW_PWR_OK cannot be
 * detected here. It can also block for many seconds in that case, which is why
 * the elapsed time is logged: a slow handshake would otherwise look like a hung
 * power cycle.
 */
static void switch_power(int on)
{
#if defined(CONFIG_BOARD_MEC172X_ADL_N_CP)
	int64_t ref = k_uptime_get();
	uint32_t elapsed;

	switch_card_power_control(on);

	elapsed = (uint32_t)k_uptime_delta(&ref);
	if (elapsed > POLL_INTERVAL_MS) {
		LOG_INF("Switch card %s handshake took %u ms",
			on ? "on" : "off", elapsed);
	}
#else
	ARG_UNUSED(on);
#endif
}

/*
 * Step 2. Forced power off: assert PM_PWRBTN and hold it until the PCH honours
 * the override, then release. No ACPI request is sent and no graceful wait is
 * attempted.
 *
 * The button must be released as soon as the PCH acts. Holding it across the
 * platform's S5 entry would be seen as a power on request and the host would
 * come back up uncommanded.
 */
static int host_force_off(void)
{
	int ret;

	LOG_DBG("Step 2: asserting power button override");

	gpio_write_pin(PM_PWRBTN, 0);
	ret = wait_for(host_going_down, CONFIG_LOM_PWRCYCLE_FORCEOFF_HOLD_MS);
	gpio_write_pin(PM_PWRBTN, 1);

	if (ret) {
		LOG_ERR("Step 2 failed: no PLTRST within %d ms",
			CONFIG_LOM_PWRCYCLE_FORCEOFF_HOLD_MS);
		return ret;
	}

	LOG_DBG("Step 2 done: power button released, host going down");

	return 0;
}

static void pwrcycle_run(void)
{
	LOG_INF("Power cycle requested");

	/* Step 2: force the host down. */
	if (host_force_off()) {
		LOG_ERR("Power cycle abandoned, host still running");
		return;
	}

	/* Step 3: confirm the host is actually down. */
	if (wait_for(host_is_down, CONFIG_LOM_PWRCYCLE_DOWN_WAIT_MS)) {
		LOG_ERR("Step 3 failed: host not confirmed down within %d ms "
			"(state %d). Power cycle abandoned",
			CONFIG_LOM_PWRCYCLE_DOWN_WAIT_MS,
			pwrseq_system_state());
		return;
	}

	LOG_INF("Step 3 done: host confirmed down");

	/* Step 4: remove switch card power. */
	switch_power(0);

	/* Step 5: dwell, measured from the confirmed switch power down. */
	k_msleep(CONFIG_LOM_PWRCYCLE_DWELL_MS);

	/*
	 * Step 6: restore switch card power before the host, so the switch is
	 * up well ahead of PCIe enumeration.
	 *
	 * power_on() also enables the switch, after ALL_SYS_PWRGD and before
	 * PCH_PWROK, so it is already ahead of the CPU leaving reset. Doing it
	 * here as well simply grants the maximum lead time; the later call then
	 * finds the pin already driven and SW_PWR_OK already high.
	 */
#ifdef CONFIG_LOM_PWRCYCLE_SWITCH_ON_BEFORE_CPU
	switch_power(1);
#endif

	/*
	 * Step 7: power the host on.
	 *
	 * Re-checked because the dwell is seconds long and pressing the power
	 * button at a host that is no longer down would request a shutdown
	 * rather than a power up.
	 */
	if (!host_is_down()) {
		LOG_WRN("Step 7 skipped: host no longer down (state %d)",
			pwrseq_system_state());
		return;
	}

	pwrbtn_pulse();

	if (wait_for(host_is_up, CONFIG_LOM_PWRCYCLE_UP_WAIT_MS)) {
		LOG_ERR("Step 7 failed: host did not reach S0 within %d ms "
			"(state %d)", CONFIG_LOM_PWRCYCLE_UP_WAIT_MS,
			pwrseq_system_state());
		return;
	}

	LOG_INF("Power cycle complete");
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
