// Simulator LVGL config: the firmware's include/lv_conf.h with the SDL
// window/input driver switched on. Everything else (fonts, memory size,
// widgets) stays identical to the device build.
#ifndef FLUIDTOUCH_SIM_LV_CONF_H
#define FLUIDTOUCH_SIM_LV_CONF_H

#include "../../include/lv_conf.h"

#undef LV_USE_SDL
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_ACCELERATED 1
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1

#endif
