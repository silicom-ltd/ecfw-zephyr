target_sources_ifdef(CONFIG_LOM_MGMT_PROC app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lom_mgmt_proc.c
    ${CMAKE_CURRENT_LIST_DIR}/host_event.c
    ${CMAKE_CURRENT_LIST_DIR}/host_event_logger.c
    ${CMAKE_CURRENT_LIST_DIR}/postcode.c
    ${CMAKE_CURRENT_LIST_DIR}/pwrctrl.c
    ${CMAKE_CURRENT_LIST_DIR}/mgmt_led_ctrl.c
    ${CMAKE_CURRENT_LIST_DIR}/hwmon_sens_${CONFIG_LOM_MGMT_SENS_TABLE}.c
    )

target_include_directories(app
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    )
