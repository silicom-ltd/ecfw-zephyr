# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thermal_v6)

# The controller is pure integer C with no Zephyr and no I/O, so the test builds
# the very same source the EC application links.
target_sources(app
    PRIVATE
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/../../app/thermal_management_v6/fan_profile_v6.c
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../app/thermal_management_v6
    )
