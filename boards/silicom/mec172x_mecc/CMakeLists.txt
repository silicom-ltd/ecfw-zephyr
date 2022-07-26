# SPDX-License-Identifier: Apache-2.0

target_sources_ifdef(CONFIG_MEC172X_ON_AZBEACH app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/azbeach_mec172x.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/azbeach_mec172x.h
    )
