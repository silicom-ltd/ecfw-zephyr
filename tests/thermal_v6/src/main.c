/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include "fan_profile_v6.h"

/* One expected cycle of a golden vector. */
struct golden_cycle {
	int input;		/* max_sw fed to the controller */
	int avg;		/* floored window average */
	uint8_t down_count;	/* consecutive-drop counter after the cycle */
	int commanded;		/* fan % commanded, or FAN_PROFILE_V6_NO_CHANGE */
	uint8_t fan;		/* fan % in force after the cycle */
};

/* Golden vector A - the worked example printed in the MHO200 thermal profile
 * document, 19 cycles. Zero wrong-direction steps: this is the controller
 * behaving correctly on a trending profile.
 *
 * Cycles 12, 13 and 14 are three consecutive DOWN commits, only reachable
 * because down_count resets to 1 rather than 0.
 */
static const struct golden_cycle golden_a[] = {
	/* in   avg  dc  commanded                  fan */
	{  36,  36,  0,                         50,  50 },	/* UP */
	{  40,  38,  0,                         50,  50 },	/* UP */
	{  43,  39,  0,                         50,  50 },	/* UP */
	{  46,  43,  0,                         50,  50 },	/* UP */
	{  47,  45,  0,                         50,  50 },	/* UP */
	{  48,  47,  0,                         70,  70 },	/* UP */
	{  49,  48,  0,                         80,  80 },	/* UP */
	{  50,  49,  0,                         90,  90 },	/* UP */
	{  50,  49,  0, FAN_PROFILE_V6_NO_CHANGE,    90 },	/* EQUAL */
	{  49,  49,  0, FAN_PROFILE_V6_NO_CHANGE,    90 },	/* EQUAL */
	{  48,  49,  0, FAN_PROFILE_V6_NO_CHANGE,    90 },	/* EQUAL */
	{  46,  47,  1, FAN_PROFILE_V6_NO_CHANGE,    90 },	/* DOWN hold */
	{  44,  46,  1,                         70,  70 },	/* DOWN commit, calc(47) */
	{  42,  44,  1,                         50,  50 },	/* DOWN commit, calc(45) */
	{  43,  43,  1,                         50,  50 },	/* DOWN commit, calc(44) */
	{  45,  43,  1, FAN_PROFILE_V6_NO_CHANGE,    50 },	/* EQUAL, keeps the streak */
	{  48,  45,  0,                         50,  50 },	/* UP */
	{  50,  47,  0,                         70,  70 },	/* UP */
	{  51,  49,  0,                         90,  90 },	/* UP */
};

/* Golden vector B - oscillating input, 9 cycles.
 *
 * Four of these steps move the fan DOWN on a cycle where the average moved UP
 * (cycles 2, 4, 6 and 8: 100 -> 90 -> 80 -> 70 -> 60 while the average climbs
 * 48 -> 49, 47 -> 48, 46 -> 47, 45 -> 46). That is not a broken test. It is
 * real behaviour of this controller: the branch keys off the direction of the
 * average, but the UP branch then sets the fan from the absolute average, so a
 * rising-but-lower average lowers the fan. This vector exists to lock that in.
 */
static const struct golden_cycle golden_b[] = {
	/* in   avg  dc  commanded                  fan */
	{  50,  50,  0,                        100, 100 },	/* UP */
	{  47,  48,  1, FAN_PROFILE_V6_NO_CHANGE,   100 },	/* DOWN hold */
	{  50,  49,  0,                         90,  90 },	/* UP, fan steps down */
	{  45,  47,  1, FAN_PROFILE_V6_NO_CHANGE,    90 },	/* DOWN hold */
	{  49,  48,  0,                         80,  80 },	/* UP, fan steps down */
	{  46,  46,  1, FAN_PROFILE_V6_NO_CHANGE,    80 },	/* DOWN hold */
	{  46,  47,  0,                         70,  70 },	/* UP, fan steps down */
	{  44,  45,  1, FAN_PROFILE_V6_NO_CHANGE,    70 },	/* DOWN hold */
	{  49,  46,  0,                         60,  60 },	/* UP, fan steps down */
};

/* Replay a vector cycle for cycle, checking every column. */
static void run_golden(struct fan_profile_v6 *p, const struct golden_cycle *v,
		       int len, const char *name)
{
	int i;

	for (i = 0; i < len; i++) {
		int fan = fan_profile_v6_step(p, v[i].input);

		zassert_equal(fan, v[i].commanded,
			"%s cycle %d: commanded %d, expected %d",
			name, i, fan, v[i].commanded);

		/* Every branch leaves last_avarage holding this cycle's
		 * average: UP and DOWN write it, and EQUAL means it already
		 * held that value.
		 */
		zassert_equal(p->last_avarage, v[i].avg,
			"%s cycle %d: avg %d, expected %d",
			name, i, p->last_avarage, v[i].avg);

		zassert_equal(p->down_count, v[i].down_count,
			"%s cycle %d: down_count %d, expected %d",
			name, i, p->down_count, v[i].down_count);

		zassert_equal(p->last_fan, v[i].fan,
			"%s cycle %d: fan in force %d, expected %d",
			name, i, p->last_fan, v[i].fan);
	}
}

ZTEST_SUITE(thermal_v6, NULL, NULL, NULL, NULL, NULL);

ZTEST(thermal_v6, test_golden_vector_a)
{
	struct fan_profile_v6 p;

	fan_profile_v6_reset(&p);
	run_golden(&p, golden_a, ARRAY_SIZE(golden_a), "vector A");
}

ZTEST(thermal_v6, test_golden_vector_b)
{
	struct fan_profile_v6 p;

	fan_profile_v6_reset(&p);
	run_golden(&p, golden_b, ARRAY_SIZE(golden_b), "vector B");
}

/* Cycles 0 and 1 divide by the number of samples taken so far, not by the full
 * window. Dividing by 3 throughout would give 20 and 40 here instead of 60.
 */
ZTEST(thermal_v6, test_partial_window)
{
	struct fan_profile_v6 p;

	fan_profile_v6_reset(&p);

	zassert_equal(fan_profile_v6_step(&p, 60), 100, "cycle 0 divided by 1");
	zassert_equal(p.last_avarage, 60, "cycle 0 average");
	zassert_equal(p.count, 1, "cycle 0 window length");

	zassert_equal(fan_profile_v6_step(&p, 60), FAN_PROFILE_V6_NO_CHANGE,
		"cycle 1 is EQUAL, so it commands nothing");
	zassert_equal(p.last_avarage, 60, "cycle 1 divided by 2");
	zassert_equal(p.count, 2, "cycle 1 window length");

	zassert_equal(fan_profile_v6_step(&p, 60), FAN_PROFILE_V6_NO_CHANGE,
		"cycle 2 is EQUAL");
	zassert_equal(p.count, 3, "window saturates at 3");

	/* And it stays saturated however long the run goes on. */
	zassert_equal(fan_profile_v6_step(&p, 60), FAN_PROFILE_V6_NO_CHANGE, NULL);
	zassert_equal(p.count, 3, "window stays at 3");
}

/* last_avarage initialises to 0, so cycle 0 always takes the UP branch - even
 * from a cold board where the first reading is well below the idle floor.
 */
ZTEST(thermal_v6, test_cold_start)
{
	struct fan_profile_v6 p;

	fan_profile_v6_reset(&p);

	zassert_equal(p.last_avarage, 0, "cold start average");
	zassert_equal(p.down_count, 0, "cold start drop counter");

	zassert_equal(fan_profile_v6_step(&p, 20), 50,
		"cycle 0 takes UP and commands the idle floor");
	zassert_equal(p.last_avarage, 20, NULL);
	zassert_equal(p.down_count, 0, NULL);
}

/* The reset entry point must restore fresh-process behaviour: the reference
 * implementation keeps state for the process lifetime, so back-to-back runs
 * contaminate each other.
 */
ZTEST(thermal_v6, test_reset_restores_fresh_process)
{
	struct fan_profile_v6 fresh;
	struct fan_profile_v6 reused;
	int i;

	/* A fresh instance and a reset instance must agree cycle for cycle. */
	fan_profile_v6_reset(&fresh);
	run_golden(&fresh, golden_a, ARRAY_SIZE(golden_a), "fresh vector A");

	fan_profile_v6_reset(&reused);
	run_golden(&reused, golden_b, ARRAY_SIZE(golden_b), "vector B first");

	/* Contaminated: without a reset, vector A no longer starts on UP,
	 * because last_avarage still holds vector B's final average of 46.
	 */
	zassert_equal(reused.last_avarage, 46, "vector B leaves 46 behind");
	zassert_not_equal(fan_profile_v6_step(&reused, golden_a[0].input),
		golden_a[0].commanded,
		"without a reset, cycle 0 must not behave like a cold start");

	fan_profile_v6_reset(&reused);

	zassert_equal(reused.last_avarage, 0, "reset clears last_avarage");
	zassert_equal(reused.down_count, 0, "reset clears down_count");
	zassert_equal(reused.count, 0, "reset clears max_history");
	for (i = 0; i < FAN_PROFILE_V6_WINDOW; i++) {
		zassert_equal(reused.max_history[i], 0, NULL);
	}

	run_golden(&reused, golden_a, ARRAY_SIZE(golden_a), "reset vector A");

	zassert_equal(reused.last_avarage, fresh.last_avarage, NULL);
	zassert_equal(reused.down_count, fresh.down_count, NULL);
	zassert_equal(reused.last_fan, fresh.last_fan, NULL);
}

ZTEST(thermal_v6, test_fan_table)
{
	zassert_equal(fan_profile_v6_calc_fan_speed(51), 100, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(50), 100, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(49), 90, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(48), 80, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(47), 70, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(46), 60, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(45), 50, NULL);

	/* Fans never go below 50%, however cold it gets. */
	zassert_equal(fan_profile_v6_calc_fan_speed(0), 50, NULL);

	/* The down path calls calc(avg + 1), which the one-degree buckets make
	 * exactly one level higher.
	 */
	zassert_equal(fan_profile_v6_calc_fan_speed(46 + 1), 70, NULL);
	zassert_equal(fan_profile_v6_calc_fan_speed(48 + 1), 90, NULL);
}

/* The averaging step relies on C integer division matching floor(). That holds
 * for a non-negative sum only - assert it rather than assume it, and pin down
 * what happens on the negative side so the non-negative requirement on the
 * input is visible here and not just in a comment.
 */
ZTEST(thermal_v6, test_integer_division_truncates_like_floor)
{
	int sum;

	for (sum = 0; sum <= 3 * 200; sum++) {
		int q = sum / FAN_PROFILE_V6_WINDOW;

		/* q == floor(sum/3) exactly when q*3 <= sum < (q+1)*3. */
		zassert_true(q * FAN_PROFILE_V6_WINDOW <= sum,
			"sum %d: %d is above floor", sum, q);
		zassert_true(sum < (q + 1) * FAN_PROFILE_V6_WINDOW,
			"sum %d: %d is below floor", sum, q);
	}

	/* For a negative sum C truncates toward zero, which is the ceiling, not
	 * the floor: -5/3 is -1 where floor(-5/3) is -2. The caller clamps
	 * negative readings to 0 to keep the window out of this range.
	 */
	zassert_equal(-5 / FAN_PROFILE_V6_WINDOW, -1,
		"C truncates toward zero on the negative side");
}

/* Truncation, not rounding: a window summing to 122 averages 40, not 41. */
ZTEST(thermal_v6, test_average_truncates_not_rounds)
{
	struct fan_profile_v6 p;

	fan_profile_v6_reset(&p);

	zassert_equal(fan_profile_v6_step(&p, 40), 50, NULL);
	zassert_equal(p.last_avarage, 40, NULL);

	/* {40, 41} -> 81/2 = 40 */
	zassert_equal(fan_profile_v6_step(&p, 41), FAN_PROFILE_V6_NO_CHANGE, NULL);
	zassert_equal(p.last_avarage, 40, NULL);

	/* {40, 41, 41} -> 122/3 = 40.67, truncated to 40. Rounding would give
	 * 41 and take the UP branch instead of EQUAL.
	 */
	zassert_equal(fan_profile_v6_step(&p, 41), FAN_PROFILE_V6_NO_CHANGE,
		"122/3 must truncate to 40 and stay EQUAL");
	zassert_equal(p.last_avarage, 40, NULL);
}
