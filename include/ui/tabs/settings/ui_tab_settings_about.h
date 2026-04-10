#ifndef UI_TAB_SETTINGS_ABOUT_H
#define UI_TAB_SETTINGS_ABOUT_H

#include <lvgl.h>

class UITabSettingsAbout {
public:
    static void create(lv_obj_t *tab);
    static void update();

private:
    static lv_obj_t *screenshot_link_label;
    static lv_obj_t *screenshot_qr;
    static lv_obj_t *lbl_fluidnc_version;  // FluidNC firmware version label
    static bool wifi_url_set;  // Track if we've already set the WiFi URL
};

#endif // UI_TAB_SETTINGS_ABOUT_H
