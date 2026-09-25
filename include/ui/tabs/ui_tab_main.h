#ifndef UI_TAB_MAIN_H
#define UI_TAB_MAIN_H

#include <lvgl.h>
#include <Arduino.h>
#include "config.h"

// Consolidated single-screen tab for reduced-axis machines (e.g. a CNC miter
// saw stop or table saw fence with only an X axis). Used instead of
// UITabStatus - and the top-level tab is labeled "Main" instead of "Status" -
// when both Y and Z are disabled for the selected machine (see
// UICommon::isYAxisEnabled() / isZAxisEnabled()).
//
// Everything needed for everyday use is on this one tab: X position (tap to
// go to a position), jogging with the Jog settings' step sizes and feed,
// a per-machine list of saved positions (Save the current position, tap to
// go there, long-press to delete), plus STOP / Home X / Zero X / Unlock.
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

    // Jogging - step sizes and feed come from the Jog settings (XY values)
    static const int MAX_STEPS = 5;
    static float step_values[MAX_STEPS];
    static int step_count;
    static int step_index;
    static lv_obj_t *step_buttons[MAX_STEPS];
    static void loadStepValues();
    static void updateStepButtonStyles();
    static void onStepClicked(lv_event_t *e);
    static void onJogClicked(lv_event_t *e);   // user_data: +1 or -1

    // Saved positions (per machine, stored in Preferences as m<i>_qpos)
    static const int MAX_SAVED = 8;
    static float saved_positions[MAX_SAVED];
    static int saved_count;
    static lv_obj_t *saved_buttons[MAX_SAVED];
    static lv_obj_t *lbl_saved_hint;
    static int pending_delete_index;
    static void loadSavedPositions();
    static void storeSavedPositions();
    static void refreshSavedButtons();
    static void goToWorkX(float x);
    static void onSaveClicked(lv_event_t *e);
    static void onSavedClicked(lv_event_t *e);      // short click: go to
    static void onSavedLongPressed(lv_event_t *e);  // long press: delete
    static void showDeleteDialog(int index);
    static void updateSavedHighlight();  // Border on the saved position matching the current X

    // State label shrinks its font to fit long states ("DISCONNECTED") in its column
    static const int STATE_MAX_WIDTH = 176;
    static void fitStateLabel(const char *state);

    // Short-lived notices from this tab, held so status updates don't overwrite them
    static const uint32_t NOTICE_HOLD_MS = 3000;
    static uint32_t notice_until_ms;
    static void showNotice(const char *message);

    // Action button handlers
    static void onHomeXClicked(lv_event_t *e);
    static void onZeroXClicked(lv_event_t *e);
    static void onUnlockClicked(lv_event_t *e);
    static void onQuickStopClicked(lv_event_t *e);
};

#endif // UI_TAB_MAIN_H
