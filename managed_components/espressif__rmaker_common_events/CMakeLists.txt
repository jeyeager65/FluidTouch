set(include_dirs "include")
set(srcs "src/common_events.c")
set(requires esp_event)

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES ${requires})
