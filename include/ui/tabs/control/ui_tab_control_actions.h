#ifndef UI_TAB_CONTROL_ACTIONS_H
#define UI_TAB_CONTROL_ACTIONS_H

#include <lvgl.h>

class UITabControlActions {
public:
    static void create(lv_obj_t *tab);

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
    static void onZeroAllClicked(lv_event_t *e);
    static void onQuickStopClicked(lv_event_t *e);
    
    // State tracking
    static bool is_paused;
    static lv_obj_t *btn_pause;
    static lv_obj_t *lbl_pause;
};

#endif // UI_TAB_CONTROL_ACTIONS_H
