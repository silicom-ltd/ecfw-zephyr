# SPDX-License-Identifier: Apache-2.0

zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/pwm.h)

zephyr_library()

zephyr_library_sources(rpm2pwm_mchp_xec.c)
