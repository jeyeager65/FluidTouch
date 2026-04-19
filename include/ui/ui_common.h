#ifndef UI_COMMON_H
#define UI_COMMON_H

#include <lvgl.h>
#include "config.h"

// Forward declaration
class DisplayDriver;

// UI state and shared objects
class UICommon {
public:
    static void init(lv_display_t *disp);
    static void setDisplayDriver(DisplayDriver* driver);  // Set display driver reference
    static void createMainUI();  // Creates main UI screen, status bar, and tabs
    static void createStatusBar();
    
    // Update functions for status bar
    static void updateModalStates(const char *text);
    static void updateMachinePosition(float x, float y, float z);
    static void updateWorkPosition(float x, float y, float z, float a = -9999.0f);
    static void updateMachineState(const char *state);
    static void updateConnectionStatus(bool machine_connected, bool wifi_connected);
    static void updateBattery(uint8_t percentage, int state);  // state: 0=discharging, 1=charging, 2=charged
    static void updateFileProgress(bool is_printing, float percent, const char *filename, uint32_t elapsed_ms);
    
    // Dialog functions
    static void showMachineSelectConfirmDialog();
    static void hideMachineSelectConfirmDialog();
    static void showPowerOffConfirmDialog();
    static void showConnectingPopup(const char *machine_name, const char *ssid);
    static void hideConnectingPopup();
    static void showConnectionErrorDialog(const char *title, const char *message);
    static void hideConnectionErrorDialog();
    static void checkConnectionTimeout();  // Non-blocking timeout check
    
    // State popup functions (HOLD and ALARM)
    static void showHoldPopup(const char *message);
    static void hideHoldPopup();
    static void showAlarmPopup(const char *message);
    static void hideAlarmPopup();
    static void checkStatePopups(int current_state, const char *last_message);  // Called from main loop
    
    // WCS lock confirmation dialog
    static void showWCSLockDialog(const char *wcs_code, const char *wcs_name, void (*continue_callback)(lv_event_t*));

    // System preferences caching (load once at startup, access via getters)
    static void loadSystemPreferences();
    static bool isAAxisEnabled();
    static void setAAxisEnabled(bool enabled);

    // Getters for shared objects
    static lv_obj_t* getStatusBar() { return status_bar; }
    static lv_display_t* getDisplay() { return display; }
    static DisplayDriver* getDisplayDriver() { return display_driver; }
    
private:
    static lv_display_t *display;
    static DisplayDriver *display_driver;
    static lv_obj_t *status_bar;
    static lv_obj_t *status_bar_left_area;   // Clickable area for Status tab
    static lv_obj_t *status_bar_right_area;  // Clickable area for machine selection
    static lv_obj_t *machine_select_dialog;  // Confirmation dialog
    static lv_obj_t *connecting_popup;       // Connecting popup
    static lv_obj_t *connection_error_dialog; // Connection error dialog
    static lv_obj_t *hold_popup;             // HOLD state popup
    static lv_obj_t *hold_popup_msg_label;   // Message label inside HOLD popup (for live updates)
    static lv_obj_t *alarm_popup;            // ALARM state popup
    static lv_obj_t *alarm_popup_msg_label;  // Message label inside ALARM popup (for live updates)
    static int last_popup_state;             // Track last state to detect changes
    static bool hold_popup_dismissed;        // User dismissed HOLD popup
    static bool alarm_popup_dismissed;       // User dismissed ALARM popup
    static lv_obj_t *lbl_modal_states;
    static lv_obj_t *lbl_status;
    
    // Connection status labels (symbols only)
    static lv_obj_t *lbl_machine_symbol;
    static lv_obj_t *lbl_machine_name;
    static lv_obj_t *lbl_wifi_symbol;
    static lv_obj_t *lbl_wifi_name;
    
    // Work Position labels (individual axes)
    static lv_obj_t *lbl_wpos_label;
    static lv_obj_t *lbl_wpos_x;
    static lv_obj_t *lbl_wpos_y;
    static lv_obj_t *lbl_wpos_z;
    static lv_obj_t *lbl_wpos_a;

    // Machine Position labels (individual axes)
    static lv_obj_t *lbl_mpos_label;
    static lv_obj_t *lbl_mpos_x;
    static lv_obj_t *lbl_mpos_y;
    static lv_obj_t *lbl_mpos_z;
    
    // Job progress display (in status bar left area)
    static lv_obj_t *lbl_file_progress_container;
    static lv_obj_t *lbl_filename;
    static lv_obj_t *bar_progress;
    static lv_obj_t *lbl_percent;
    static lv_obj_t *lbl_elapsed_time;
    static lv_obj_t *lbl_elapsed_unit;
    static lv_obj_t *lbl_estimated_time;
    static lv_obj_t *lbl_estimated_unit;
    
    // Cached values for delta checking (prevent unnecessary redraws)
    static float last_wpos_x, last_wpos_y, last_wpos_z, last_wpos_a;
    static float last_mpos_x, last_mpos_y, last_mpos_z;

    // Battery indicator (compact graphical widget: outline + fill bar + nub + % label)
    static lv_obj_t *battery_body;        // Battery outline rectangle
    static lv_obj_t *battery_fill;        // Inner fill bar (lv_bar)
    static lv_obj_t *battery_nub;         // Small nub on the right side
    static lv_obj_t *lbl_battery_charge;  // Lightning bolt overlay when charging
    static lv_obj_t *lbl_battery_percent; // Percentage text next to battery
    static uint8_t last_battery_pct;
    static int last_battery_state;

    // Cached system preferences (loaded once at startup)
    static bool enable_a_axis;
};

#endif // UI_COMMON_H
