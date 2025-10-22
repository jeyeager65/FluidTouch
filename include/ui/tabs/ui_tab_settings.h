#ifndef UI_TAB_SETTINGS_H
#define UI_TAB_SETTINGS_H

#include <lvgl.h>

class UITabSettings {
public:
    static void create(lv_obj_t *tab);
private:
    static void createGeneralTab(lv_obj_t *tab);
    static void createConnectionTab(lv_obj_t *tab);
    static void createFluidNCTab(lv_obj_t *tab);
};

#endif // UI_TAB_SETTINGS_H
