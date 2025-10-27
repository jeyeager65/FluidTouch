#ifndef UI_TAB_SETTINGS_JOG_H
#define UI_TAB_SETTINGS_JOG_H

#include <lvgl.h>

class UITabSettingsJog {
public:
    static void create(lv_obj_t *tab);
    
    // Load/save preferences
    static void loadPreferences();
    static void savePreferences();
    
    // Keyboard management
    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();
    
    // Getters for default values
    static float getDefaultXYStep();
    static float getDefaultZStep();
    static int getDefaultXYFeed();
    static int getDefaultZFeed();
    static int getMaxXYFeed();
    static int getMaxZFeed();
    
    // Setters for updating values (used by event handlers)
    static void setDefaultXYStep(float value);
    static void setDefaultZStep(float value);
    static void setDefaultXYFeed(int value);
    static void setDefaultZFeed(int value);
    static void setMaxXYFeed(int value);
    static void setMaxZFeed(int value);

private:
    // Default values
    static float default_xy_step;
    static float default_z_step;
    static int default_xy_feed;
    static int default_z_feed;
    static int max_xy_feed;
    static int max_z_feed;
    
    // Keyboard
    static lv_obj_t *keyboard;
    static lv_obj_t *parent_tab;  // Store parent tab for keyboard positioning
};

#endif // UI_TAB_SETTINGS_JOG_H
