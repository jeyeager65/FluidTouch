# FluidTouch desktop simulator
#
# Builds the real FluidTouch UI + FluidNC client (src/) as a Windows/Linux
# desktop app. The ESP32/Arduino APIs the firmware uses are replaced by the
# desktop implementations in sim/include and sim/src, and the display/touch
# drivers are replaced by an SDL2 window. See sim/README.md.

cmake_minimum_required(VERSION 3.20)
project(FluidTouchSim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

get_filename_component(FT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(SIM_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# ---------------------------------------------------------------------------
# Third-party libraries - same versions as platformio.ini lib_deps
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.5.0
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _no_cmake_)
FetchContent_Declare(arduinojson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v7.4.3
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _no_cmake_)
FetchContent_Declare(arduinowebsockets
    GIT_REPOSITORY https://github.com/gilmaimon/ArduinoWebsockets.git
    GIT_TAG        0.5.4
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  _no_cmake_)
# ESP-NOW pairing crypto; the ESP32 framework ships mbedtls 3.6 too
FetchContent_Declare(mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.7
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES ""
    SOURCE_SUBDIR  _no_cmake_)

# Download sources only (SOURCE_SUBDIR points at a non-existent dir so their
# own CMake projects aren't added) - we compile them ourselves below.
FetchContent_MakeAvailable(lvgl arduinojson arduinowebsockets mbedtls)

find_package(SDL2 REQUIRED CONFIG)

# ---------------------------------------------------------------------------
# Shared compile settings
# ---------------------------------------------------------------------------
# sim/include MUST come before include/ so the desktop Arduino/ESP32 headers
# and the SDL display_driver.h override the hardware versions.
set(FT_INCLUDE_DIRS
    "${SIM_DIR}/include"
    "${FT_ROOT}/include"
    "${lvgl_SOURCE_DIR}"
    "${lvgl_SOURCE_DIR}/.."
    "${arduinojson_SOURCE_DIR}/src"
    "${arduinowebsockets_SOURCE_DIR}/src")

set(FT_DEFINES
    FLUIDTOUCH_SIM=1
    LV_CONF_INCLUDE_SIMPLE=1
    LV_LVGL_H_INCLUDE_SIMPLE=1
    SDL_MAIN_HANDLED=1
    VERSION="1.1.0-sim"
    # ArduinoJson: enable String/Stream/Print support without defining ARDUINO
    ARDUINOJSON_ENABLE_ARDUINO_STRING=1
    ARDUINOJSON_ENABLE_ARDUINO_STREAM=1
    ARDUINOJSON_ENABLE_ARDUINO_PRINT=1
    ARDUINOJSON_ENABLE_PROGMEM=0
    # ArduinoWebsockets: no TLS (FluidNC is plain ws://)
    _WS_CONFIG_NO_SSL=1)

# Sim-side ArduinoWebsockets platform glue (defines WSDefaultTcpClient) is
# force-included into every C++ file, since the library only knows ESP/Teensy.
set(FT_CXX_FORCE_INCLUDE "${SIM_DIR}/include/sim_ws_platform.h")

# ---------------------------------------------------------------------------
# LVGL (compiled from source with the firmware's lv_conf.h + SDL enabled)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${lvgl_SOURCE_DIR}/src/*.c")
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${FT_INCLUDE_DIRS})
target_compile_definitions(lvgl PUBLIC ${FT_DEFINES})
target_link_libraries(lvgl PUBLIC SDL2::SDL2)
target_compile_options(lvgl PRIVATE -w -O2 -funsigned-char)

# ---------------------------------------------------------------------------
# ArduinoWebsockets
# ---------------------------------------------------------------------------
file(GLOB WS_SOURCES "${arduinowebsockets_SOURCE_DIR}/src/*.cpp")
add_library(arduinowebsockets STATIC ${WS_SOURCES})
target_include_directories(arduinowebsockets PUBLIC ${FT_INCLUDE_DIRS})
target_compile_definitions(arduinowebsockets PUBLIC ${FT_DEFINES})
target_compile_options(arduinowebsockets PRIVATE -w -funsigned-char
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${FT_CXX_FORCE_INCLUDE}>")

# ---------------------------------------------------------------------------
# mbedtls crypto (no TLS/X.509 needed)
# ---------------------------------------------------------------------------
file(GLOB MBEDTLS_SOURCES "${mbedtls_SOURCE_DIR}/library/*.c")
list(FILTER MBEDTLS_SOURCES EXCLUDE REGEX ".*/library/(net_sockets|ssl_|x509|pkcs7|debug|mps_).*\\.c$")
add_library(mbedcrypto STATIC ${MBEDTLS_SOURCES})
target_include_directories(mbedcrypto
    PUBLIC "${mbedtls_SOURCE_DIR}/include"
    PRIVATE "${mbedtls_SOURCE_DIR}/library")
target_compile_options(mbedcrypto PRIVATE -w -O2)
if(WIN32)
    target_link_libraries(mbedcrypto PUBLIC bcrypt)
endif()

# ---------------------------------------------------------------------------
# FluidTouch app: firmware sources + simulator platform layer
# ---------------------------------------------------------------------------
file(GLOB_RECURSE FT_SOURCES CONFIGURE_DEPENDS
    "${FT_ROOT}/src/*.cpp"
    "${FT_ROOT}/src/*.c")

# Hardware-only sources replaced by sim/src equivalents
list(FILTER FT_SOURCES EXCLUDE REGEX ".*/src/core/display_driver\\.cpp$")
list(FILTER FT_SOURCES EXCLUDE REGEX ".*/src/core/touch_driver\\.cpp$")
list(FILTER FT_SOURCES EXCLUDE REGEX ".*/src/network/screenshot_server\\.cpp$")

file(GLOB SIM_SOURCES CONFIGURE_DEPENDS "${SIM_DIR}/src/*.cpp")

add_executable(fluidtouch_sim ${FT_SOURCES} ${SIM_SOURCES})
target_include_directories(fluidtouch_sim PRIVATE ${FT_INCLUDE_DIRS})
target_compile_definitions(fluidtouch_sim PRIVATE
    ${FT_DEFINES}
    SIM_DEFAULT_DATA_DIR="${SIM_DIR}/data")
target_compile_options(fluidtouch_sim PRIVATE
    -funsigned-char  # Xtensa (ESP32) char is unsigned; firmware relies on it
    -Wall -Wno-unused-variable -Wno-unused-function -Wno-format
    -Wno-unused-but-set-variable -Wno-missing-field-initializers
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${FT_CXX_FORCE_INCLUDE}>")
target_link_libraries(fluidtouch_sim PRIVATE lvgl arduinowebsockets mbedcrypto SDL2::SDL2)

# ESP-NOW link test against sim/tools/fake_fluidnc.py (see sim/test/espnow_link_test.cpp)
add_executable(espnow_link_test
    "${SIM_DIR}/test/espnow_link_test.cpp"
    "${SIM_DIR}/src/arduino_core.cpp"
    "${SIM_DIR}/src/espnow_radio_udp.cpp"
    "${FT_ROOT}/src/network/espnow_link.cpp"
    "${FT_ROOT}/src/network/espnow_crypto.cpp")
target_include_directories(espnow_link_test PRIVATE ${FT_INCLUDE_DIRS})
target_compile_definitions(espnow_link_test PRIVATE
    ${FT_DEFINES}
    SIM_DEFAULT_DATA_DIR="${SIM_DIR}/data")
target_compile_options(espnow_link_test PRIVATE -funsigned-char -Wall -Wno-unused-function
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${FT_CXX_FORCE_INCLUDE}>")
target_link_libraries(espnow_link_test PRIVATE mbedcrypto)

if(WIN32)
    target_link_libraries(fluidtouch_sim PRIVATE ws2_32 iphlpapi)
    target_link_libraries(espnow_link_test PRIVATE ws2_32)
    # Static libgcc/libstdc++ so the exe only needs SDL2.dll next to it
    target_link_options(fluidtouch_sim PRIVATE -static-libgcc -static-libstdc++)
    target_link_options(espnow_link_test PRIVATE -static-libgcc -static-libstdc++)
    # Copy runtime DLLs next to the exe so it runs outside the MSYS2 shell
    # (and doesn't pick up an incompatible libwinpthread from e.g. Git's PATH)
    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    add_custom_command(TARGET fluidtouch_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            "${MINGW_BIN}/libwinpthread-1.dll"
            $<TARGET_FILE_DIR:fluidtouch_sim>)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(fluidtouch_sim PRIVATE Threads::Threads)
    target_link_libraries(espnow_link_test PRIVATE Threads::Threads)
endif()
