# SPDX-License-Identifier: Apache-2.0
#
# hwmon: consolidates the EC's board/thermal sensor and fan driver
# mechanisms - sensors.c, sensors_cp.c, fan.c, emc230x_fan.c, peci_hub.c -
# that were previously scattered across drivers/. Kept as one module since
# they share the hwmon SRAM layout (see hwmon.h) and the per-board sensor
# wiring (see hwmon_cp.h and the "silicom,board-sensors" devicetree binding).

if (CONFIG_BOARD_MEC172X_AZBEACH OR CONFIG_BOARD_MEC172X_ADL_N OR
    CONFIG_BOARD_MEC172X_ADL_N_CP)
    # peci_hub.c backs both the hwmon-style thermal sensor path below and the
    # plain thermal_sensor.c path in drivers/CMakeLists.txt, so it is added
    # here unconditionally on board rather than being tied to either style.
    target_sources(app
        PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/peci_hub.c
        PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/peci_hub.h
        )

    if (CONFIG_THERMAL_MANAGEMENT OR CONFIG_THERMAL_MANAGEMENT_V3 OR CONFIG_THERMAL_MANAGEMENT_V4 OR
        CONFIG_THERMAL_MANAGEMENT_V6)
        target_sources(app
            PRIVATE
            ${CMAKE_CURRENT_LIST_DIR}/sensors.c
            PUBLIC
            ${CMAKE_CURRENT_LIST_DIR}/hwmon.h
            )

        # sensors_cp.c reads whichever devices the active add-on card overlay
        # (snippets/mho-150, snippets/mho-200) lists on its "silicom,board-sensors"
        # node - see out_of_tree_boards/boards/arm/mec172x_adl_n_cp/dts/bindings/sensor/.
        # Built whenever that node is present, rather than being tied to a
        # specific board Kconfig symbol.
        if (CONFIG_DT_HAS_SILICOM_BOARD_SENSORS_ENABLED)
            target_sources(app
                PRIVATE
                ${CMAKE_CURRENT_LIST_DIR}/sensors_cp.c
                PUBLIC
                ${CMAKE_CURRENT_LIST_DIR}/hwmon_cp.h
                )
        endif()
    endif()
endif()

target_sources(app
    PUBLIC
    #    fan.c is superseded by emc230x_fan.c below (EMC2301x controls the
    #    fans over I2C instead of the SoC's own rpm2pwm hardware) and isn't
    #    built; kept for reference, matching drivers/CMakeLists.txt's
    #    convention for other superseded files.
    ${CMAKE_CURRENT_LIST_DIR}/fan.h
    )

target_sources_ifdef(CONFIG_SOC_SERIES_MEC172X app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/emc230x_fan.c
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/emc230x_fan.h
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
