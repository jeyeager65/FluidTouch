#ifndef UI_TAB_CONTROL_PROBE_H
#define UI_TAB_CONTROL_PROBE_H

#include <lvgl.h>

class UITabControlProbe {
public:
    static void create(lv_obj_t *tab);
    static void updateResult(const char* message);
    static void updateResult(float x, float y, float z, bool success);
    static void updateProbe(bool triggered);
    
    // Keyboard support
    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();

private:
    static lv_obj_t* results_text;
    static lv_obj_t* probe_indicator;
    static lv_obj_t* keyboard;
    static lv_obj_t* parent_tab;
    
    // Track which axis was last probed (for filtering results)
    static char last_probed_axis;
    
    // Event handlers for probe buttons
    static void probe_x_minus_handler(lv_event_t* e);
    static void probe_x_plus_handler(lv_event_t* e);
    static void probe_y_minus_handler(lv_event_t* e);
    static void probe_y_plus_handler(lv_event_t* e);
    static void probe_z_minus_handler(lv_event_t* e);
    
    // Helper to execute probe command
    static void executeProbe(char axis, char direction, bool checkWcsLock = true);
};

#endif // UI_TAB_CONTROL_PROBE_H
