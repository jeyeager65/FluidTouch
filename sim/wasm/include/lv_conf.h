// Browser (Emscripten) LVGL config: the firmware's include/lv_conf.h as is.
// No SDL - sim/wasm/display_touch_wasm.cpp renders into a framebuffer the
// page copies to its canvas.
#ifndef FLUIDTOUCH_WASM_LV_CONF_H
#define FLUIDTOUCH_WASM_LV_CONF_H

#include "../../../include/lv_conf.h"

#endif
