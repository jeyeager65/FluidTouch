set(include_dirs "include")
set(srcs "src/time_sync.c" "src/timezone.c")
set(req lwip)
set(priv_req nvs_flash esp_netif)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${req}
                       PRIV_REQUIRES ${priv_req})
