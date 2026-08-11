/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FAN_PROFILE_V6_H__
#define __FAN_PROFILE_V6_H__

#include <stdint.h>

/**
 * @file
 * @brief MHO200 adaptive fan speed controller.
 *
 * Pure integer port of the shipped thermal_profile() function, kept free of
 * Zephyr and of any I/O so the control decisions can be replayed against the
 * golden vectors in the MHO200 thermal profile document.
 *
 * Three behaviours of the reference implementation look like defects and are
 * reproduced deliberately - matching the shipped controller is the point, and
 * changing it invalidates comparison with completed oven runs:
 *
 * 1. down_count resets to 1 after a commit, not 0, so the two-cycle wait gates
 *    only the first reduction of a descent.
 * 2. The EQUAL branch returns before touching down_count, so a stable cycle
 *    preserves the streak and the two drops need not be consecutive.
 * 3. floor() is applied twice, to the sensor maximum and again to the mean.
 *
 * A consequence, also deliberate: because the branch keys off direction rather
 * than the level currently in force, an oscillating average can step the fan
 * down on a cycle where the temperature went up.
 */

/* Returned by fan_profile_v6_step() for a cycle that commands no fan change. */
#define FAN_PROFILE_V6_NO_CHANGE		(-1)

/* Number of floored maxima averaged per cycle. */
#define FAN_PROFILE_V6_WINDOW			3

/* Idle floor. Fans never go below this. */
#define FAN_PROFILE_V6_MIN_DUTY			50

struct fan_profile_v6 {
	/* max_history. Only the last FAN_PROFILE_V6_WINDOW entries are ever
	 * read, so the append-only list of the reference implementation is a
	 * ring here. count saturates at the window size: min(len, 3) is all the
	 * average needs, and a saturating count cannot wrap on a box that runs
	 * for months.
	 */
	int16_t max_history[FAN_PROFILE_V6_WINDOW];
	uint8_t next;
	uint8_t count;

	/* Previous cycle's floored average. Initialises to 0, which is what
	 * makes cycle 0 always take the UP branch.
	 */
	int16_t last_avarage;

	/* Consecutive-drop counter. Spelling of last_avarage and the name of
	 * this field are kept from the reference implementation so the two can
	 * be diffed side by side.
	 */
	uint8_t down_count;

	/* Last value handed to the fans. Kept so every cycle can log the
	 * commanded speed and not just the measured RPM. Not one of the three
	 * state variables of the algorithm.
	 */
	uint8_t last_fan;
};

/**
 * @brief Clear the controller state.
 *
 * The reference implementation keeps state for the process lifetime, so
 * back-to-back runs contaminate each other and cycle 0 behaves differently
 * depending on where the previous run stopped. Call this to start a run from
 * fresh-process behaviour.
 *
 * @param p controller instance.
 */
void fan_profile_v6_reset(struct fan_profile_v6 *p);

/**
 * @brief Fan duty cycle for a whole-degree temperature.
 *
 * Buckets are exactly one degree wide from 46C up, which is what makes the
 * down path's calc(avg + 1) precisely one level higher.
 *
 * @param t temperature in whole degrees C.
 *
 * @return fan duty cycle in percent, never below FAN_PROFILE_V6_MIN_DUTY.
 */
uint8_t fan_profile_v6_calc_fan_speed(int t);

/**
 * @brief Run one control cycle.
 *
 * @param p controller instance.
 * @param max_sw floor of the maximum of the two SW ambient thermistors and the
 *		 CPU-side ambient, in whole degrees C. Must not be negative:
 *		 the window average relies on integer division truncating the
 *		 same way floor() does, which only holds for a non-negative sum.
 *
 * @return commanded fan duty cycle in percent, or FAN_PROFILE_V6_NO_CHANGE for
 *	   a cycle that commands nothing and leaves the fans as they are.
 */
int fan_profile_v6_step(struct fan_profile_v6 *p, int max_sw);

#endif	/* __FAN_PROFILE_V6_H__ */
