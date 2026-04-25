#ifndef UI_TAB_CONTROL_ACTIONS_H
#define UI_TAB_CONTROL_ACTIONS_H

#include <lvgl.h>
#include <Arduino.h>
#include "config.h"

class UITabControlActions {
public:
    static void create(lv_obj_t *tab);
    static void updatePauseButton(int machine_state);
    static void updateLimitSwitches(bool x, bool y, bool z, bool a = false);

private:
    // Event handlers
    static void onPauseResumeClicked(lv_event_t *e);
    static void onUnlockClicked(lv_event_t *e);
    static void onSoftResetClicked(lv_event_t *e);
    static void onHomeXClicked(lv_event_t *e);
    static void onHomeYClicked(lv_event_t *e);
    static void onHomeZClicked(lv_event_t *e);
    static void onHomeAllClicked(lv_event_t *e);
    static void onZeroXClicked(lv_event_t *e);
    static void onZeroYClicked(lv_event_t *e);
    static void onZeroZClicked(lv_event_t *e);
    static void onZeroAClicked(lv_event_t *e);
    static void onZeroAllClicked(lv_event_t *e);
    static void onQuickStopClicked(lv_event_t *e);
    
    // State tracking for pause/resume button
    static lv_obj_t *btn_pause;
    static lv_obj_t *lbl_pause;
    
    // Home button references for limit switch indicators
    static lv_obj_t *btn_home_x;
    static lv_obj_t *btn_home_y;
    static lv_obj_t *btn_home_z;

    // Timestamps of last trigger per axis (millis), for visual hold
    static uint32_t last_trigger_x_ms;
    static uint32_t last_trigger_y_ms;
    static uint32_t last_trigger_z_ms;
};

#endif // UI_TAB_CONTROL_ACTIONS_H
