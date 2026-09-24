#ifndef UI_TAB_MAIN_H
#define UI_TAB_MAIN_H

#include <lvgl.h>
#include <Arduino.h>
#include "config.h"

// Consolidated single-screen tab for reduced-axis machines (e.g. a CNC miter
// saw fence with only an X axis). Used instead of UITabStatus - and the
// top-level tab is labeled "Main" instead of "Status" - when both Y and Z
// are disabled for the selected machine (see UICommon::isYAxisEnabled() /
// isZAxisEnabled()). Combines X position display/edit, Home X and Zero X
// into a single tab so the user never needs to visit the Control tab for
// everyday operation.
class UITabMain {
public:
    static void create(lv_obj_t *tab);

    // Update methods for live data - signatures mirror UITabStatus so main.cpp
    // can call both unconditionally (whichever tab wasn't created safely
    // no-ops via null checks, same pattern used throughout this codebase).
    static void updateMessage(const char *message);
    static void updateState(const char *state);
    static void updateWorkPosition(float x, float y, float z, float a = -9999.0f);
    static void updateMachinePosition(float x, float y, float z, float a = -9999.0f);
    static void updateLimitSwitches(bool x, bool y, bool z, bool a = false);

private:
    static lv_obj_t *lbl_state;
    static lv_obj_t *lbl_message;
    static lv_obj_t *ind_limit_x;

    // Position textareas (editable "Go To" fields, X axis only)
    static lv_obj_t *lbl_wpos_x;
    static lv_obj_t *lbl_mpos_x;

    // Delta-checking caches to avoid unnecessary redraws
    static float last_wpos_x;
    static float last_mpos_x;
    static char last_state[16];

    // Timestamp of last limit trigger (millis), for visual hold
    static uint32_t last_limit_trigger_x_ms;
    static lv_obj_t *btn_home_x;

    // On-screen keyboard for position editing (same pattern as UITabStatus)
    static lv_obj_t *keyboard;
    static lv_obj_t *active_textarea;
    static char original_value[32];

    static void position_field_event_handler(lv_event_t *e);
    static void keyboard_event_handler(lv_event_t *e);
    static void showValidationError(const char *message);

    // Action button handlers
    static void onHomeXClicked(lv_event_t *e);
    static void onZeroXClicked(lv_event_t *e);
    static void onUnlockClicked(lv_event_t *e);
    static void onQuickStopClicked(lv_event_t *e);
};

#endif // UI_TAB_MAIN_H
