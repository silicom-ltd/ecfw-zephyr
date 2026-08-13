/*
 * Copyright (c) 2026 Silicom Connectivity Solutions, Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fan_profile_v6.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array)	(sizeof(array) / sizeof((array)[0]))
#endif

struct fan_step {
	int16_t temp;
	uint8_t duty_cycle;
};

/* Fan table, highest bucket first. Anything below the last entry lands on the
 * FAN_PROFILE_V6_MIN_DUTY idle floor.
 */
static const struct fan_step fan_step_tbl[] = {
	{50, 100},
	{49, 90},
	{48, 80},
	{47, 70},
	{46, 60},
};

void fan_profile_v6_reset(struct fan_profile_v6 *p)
{
	int i;

	for (i = 0; i < FAN_PROFILE_V6_WINDOW; i++) {
		p->max_history[i] = 0;
	}
	p->next = 0;
	p->count = 0;

	p->last_avarage = 0;
	p->down_count = 0;

	p->last_fan = 0;
}

uint8_t fan_profile_v6_calc_fan_speed(int t)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fan_step_tbl); i++) {
		if (t >= fan_step_tbl[i].temp) {
			return fan_step_tbl[i].duty_cycle;
		}
	}

	return FAN_PROFILE_V6_MIN_DUTY;
}

int fan_profile_v6_step(struct fan_profile_v6 *p, int max_sw)
{
	int sum = 0;
	int avg;
	int i;

	/* max_sw arrives already floored: the caller takes sensor_value.val1
	 * from each thermistor, and floor(max(a, b, c)) == max(floor a, floor b,
	 * floor c) because floor is monotone, so flooring per sensor loses
	 * nothing against flooring the maximum. That is the first of the two
	 * floor() calls.
	 */
	p->max_history[p->next] = (int16_t)max_sw;
	p->next = (p->next + 1) % FAN_PROFILE_V6_WINDOW;
	if (p->count < FAN_PROFILE_V6_WINDOW) {
		p->count++;
	}

	/* Entries [0, count) are the window in both cases: while the ring is
	 * still filling they are the only entries written, and once it is full
	 * count covers all of it whatever the insertion point.
	 */
	for (i = 0; i < p->count; i++) {
		sum += p->max_history[i];
	}

	/* Second floor(). Integer division truncates toward zero, which matches
	 * floor() only for a non-negative sum - hence the non-negative
	 * requirement on max_sw. Cycles 0 and 1 divide by count, not by the full
	 * window.
	 */
	avg = sum / p->count;

	if (p->last_avarage < avg) {
		/* UP: the only branch that clears the drop counter. Note the fan
		 * is set from the absolute average, so a rising-but-lower
		 * average lowers the fan.
		 */
		p->down_count = 0;
		p->last_fan = fan_profile_v6_calc_fan_speed(avg);
		p->last_avarage = (int16_t)avg;
		return p->last_fan;
	}

	if (p->last_avarage == avg) {
		/* EQUAL: returns before touching down_count, so a stable cycle
		 * preserves the streak. Writes no state at all. Deliberate quirk
		 * - do not "fix".
		 */
		return FAN_PROFILE_V6_NO_CHANGE;
	}

	/* DOWN */
	p->down_count++;
	if (p->down_count >= 2) {
		/* calc(avg + 1) is precisely one level higher than the table
		 * entry for the new average, because the buckets are one degree
		 * wide.
		 */
		p->last_fan = fan_profile_v6_calc_fan_speed(avg + 1);

		/* Resets to 1, not 0: the two-cycle wait therefore gates only
		 * the first reduction of a descent, and every consecutive fall
		 * after that steps down. Deliberate quirk - do not "fix".
		 */
		p->down_count = 1;
		p->last_avarage = (int16_t)avg;
		return p->last_fan;
	}

	/* First drop of a descent: hold this cycle. */
	p->last_avarage = (int16_t)avg;

	return FAN_PROFILE_V6_NO_CHANGE;
}
