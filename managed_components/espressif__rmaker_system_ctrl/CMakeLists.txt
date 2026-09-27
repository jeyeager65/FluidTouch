set(include_dirs "include")
set(srcs "src/system_ctrl.c")
set(priv_req nvs_flash)

# Add esp_wifi only for non-Linux targets
if(NOT "${IDF_TARGET}" STREQUAL "linux")
    list(APPEND priv_req esp_wifi)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES ${priv_req})
