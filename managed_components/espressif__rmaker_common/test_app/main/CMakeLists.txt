set(srcs
    "test_app_main.c"
    "test_rmaker_utils.c"
    "test_work_queue.c")

set(priv_requires "unity esp_timer nvs_flash lwip esp_netif")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ${priv_requires}
                       WHOLE_ARCHIVE)
