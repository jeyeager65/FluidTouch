set(include_dirs "include")
set(srcs "src/rmaker_console.c" "src/rmaker_common_cmds.c")
set(priv_req console esp_timer nvs_flash lwip)

# driver component was split in IDF v6.0; esp_driver_uart is needed for console
if(NOT "${IDF_TARGET}" STREQUAL "linux" AND "${IDF_VERSION_MAJOR}" LESS 6)
    list(APPEND priv_req driver)
else()
    list(APPEND priv_req esp_driver_uart)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES ${priv_req})
