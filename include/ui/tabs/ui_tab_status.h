#ifndef UI_TAB_STATUS_H
#define UI_TAB_STATUS_H

#include <lvgl.h>

class UITabStatus {
public:
    static void create(lv_obj_t *tab);
    
    // Update methods for live data
    static void updateMessage(const char *message);
    static void updateState(const char *state);
    static void updateWorkPosition(float x, float y, float z, float a = -9999.0f);
    static void updateMachinePosition(float x, float y, float z, float a = -9999.0f);
    static void updateFeedRate(float rate, float override_pct);
    static void updateRapidOverride(float override_pct);
    static void updateSpindle(float speed, float override_pct);
    static void updateModalStates(const char *wcs, const char *plane, const char *dist, 
                                  const char *units, const char *motion, const char *feedrate,
                                  const char *spindle, const char *coolant, const char *tool);
    static void updateControlButtons(int machine_state);
    
private:
    static lv_obj_t *lbl_message;
    static lv_obj_t *lbl_state;
    
    // Control buttons (in place of job progress)
    static lv_obj_t *btn_pause;
    static lv_obj_t *lbl_pause;
    static lv_obj_t *btn_stop;
    static lv_obj_t *btn_cancel_jog;
    static lv_obj_t *lbl_wpos_x;
    static lv_obj_t *lbl_wpos_y;
    static lv_obj_t *lbl_wpos_z;
    static lv_obj_t *lbl_wpos_a;
    static lv_obj_t *lbl_mpos_x;
    static lv_obj_t *lbl_mpos_y;
    static lv_obj_t *lbl_mpos_z;
    static lv_obj_t *lbl_mpos_a;
    
    // Keyboards for position editing
    static lv_obj_t *keyboard;
    static lv_obj_t *active_textarea;  // Which position field is being edited
    static char original_value[32];    // Store original value for cancel restoration
    
    // Event handlers for position editing
    static void position_field_event_handler(lv_event_t *e);
    static void keyboard_event_handler(lv_event_t *e);
    static void showValidationError(const char* message);
    static lv_obj_t *lbl_feed_value;
    static lv_obj_t *lbl_feed_override;
    static lv_obj_t *lbl_feed_units;
    static lv_obj_t *lbl_rapid_override;
    static lv_obj_t *lbl_spindle_value;
    static lv_obj_t *lbl_spindle_override;
    static lv_obj_t *lbl_spindle_units;
    
    // Modal state labels
    static lv_obj_t *btn_modal_wcs;  // Changed from lbl_modal_wcs to button
    static lv_obj_t *lbl_modal_wcs_value;  // Label inside the button
    static lv_obj_t *lbl_modal_plane;
    static lv_obj_t *lbl_modal_dist;
    static lv_obj_t *lbl_modal_units;
    static lv_obj_t *lbl_modal_motion;
    static lv_obj_t *lbl_modal_feedrate;
    static lv_obj_t *lbl_modal_spindle;
    static lv_obj_t *lbl_modal_coolant;
    static int coolant_base_y;
    static lv_obj_t *lbl_modal_tool;
    
    // Cached values for delta checking (prevent unnecessary redraws)
    static float last_wpos_x, last_wpos_y, last_wpos_z, last_wpos_a;
    static float last_mpos_x, last_mpos_y, last_mpos_z, last_mpos_a;
    static float last_feed_rate, last_feed_override;
    static float last_rapid_override;
    static float last_spindle_speed, last_spindle_override;
    static char last_state[16];
    static char last_modal_wcs[8];
    static char last_modal_plane[8];
    static char last_modal_dist[8];
    static char last_modal_units[8];
    static char last_modal_motion[8];
    static char last_modal_feedrate[8];
    static char last_modal_spindle[8];
    static char last_modal_coolant[8];
    static char last_modal_tool[8];
    
    // WCS selection popup
    static lv_obj_t *wcs_popup;
    static lv_obj_t *wcs_popup_content;
    static lv_obj_t *wcs_btn_set;
    static lv_obj_t *wcs_btn_cancel;
    static lv_obj_t *wcs_buttons[6];  // Store WCS button references for highlighting
    static int selected_wcs_index;  // Track which WCS is selected (-1 = none)
    static int current_wcs_index;  // Track which WCS is currently active (-1 = none)
    static char wcs_offsets[10][64];  // Store parsed WCS offsets (G54-G59, G28, G30, G92, TLO)
    
    // Event handlers for control buttons
    static void onPauseResumeClicked(lv_event_t *e);
    static void onStopClicked(lv_event_t *e);
    static void onCancelJogClicked(lv_event_t *e);
    
    // WCS selection event handlers
    static void onWCSButtonClicked(lv_event_t *e);
    static void onWCSSelected(lv_event_t *e);
    static void onWCSSetClicked(lv_event_t *e);
    static void onWCSCancelClicked(lv_event_t *e);
    static void showWCSPopup();
    static void hideWCSPopup();
    static void parseWCSOffsetsResponse(const char* response);
};

#endif // UI_TAB_STATUS_H
