set(srcs "src/factory.c")

set(priv_req nvs_flash lwip)
set(requires esp_event)

list(APPEND priv_req esp_timer)

# mqtt component was moved to the component manager in IDF v6.0
if("${IDF_VERSION_MAJOR}" LESS 6)
    list(APPEND priv_req mqtt)
endif()

# Add esp_wifi only for non-Linux targets
if(NOT "${IDF_TARGET}" STREQUAL "linux")
    list(APPEND priv_req esp_wifi)
endif()

#if(CONFIG_ESP_RMAKER_LIB_ESP_MQTT)
    list(APPEND srcs "src/esp-mqtt/esp-mqtt-glue.c")
#endif()
if(CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME)
    list(APPEND srcs "src/create_APN3_PPI_string.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS
                       REQUIRES ${requires}
                       PRIV_REQUIRES ${priv_req})
