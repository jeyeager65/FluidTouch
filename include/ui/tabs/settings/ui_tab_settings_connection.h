#ifndef UI_TAB_SETTINGS_CONNECTION_H
#define UI_TAB_SETTINGS_CONNECTION_H

#include <lvgl.h>

class UITabSettingsConnection {
public:
    static void create(lv_obj_t *tab);
    static void loadSettings();
    static void saveSettings();
};

#endif // UI_TAB_SETTINGS_CONNECTION_H
