#ifndef UI_TABS_H
#define UI_TABS_H

#include <lvgl.h>
#include "config.h"

class UITabs {
public:
    static void createTabs();
    static void createStatusTab(lv_obj_t *tab);
    static void createControlTab(lv_obj_t *tab);
    static void createFilesTab(lv_obj_t *tab);
    static void createMacrosTab(lv_obj_t *tab);
    static void createTerminalTab(lv_obj_t *tab);
    static void createSettingsTab(lv_obj_t *tab);
    
    // Settings management
    static void loadSettings();
    static void saveSettings();
    
    // Getters for tab objects (for external access if needed)
    static lv_obj_t* getTabview() { return tabview; }
    
private:
    static lv_obj_t *tabview;
    static lv_obj_t *tab_status;
    static lv_obj_t *tab_control;
    static lv_obj_t *tab_files;
    static lv_obj_t *tab_macros;
    static lv_obj_t *tab_terminal;
    static lv_obj_t *tab_settings;
    
    // Event handler for tab changes
    static void tab_changed_event_cb(lv_event_t *e);
};

#endif // UI_TABS_H
