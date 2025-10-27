#ifndef UI_MACHINE_SELECT_H
#define UI_MACHINE_SELECT_H

#include <lvgl.h>
#include "ui/machine_config.h"

class UIMachineSelect {
public:
    static void show(lv_display_t *disp);
    static void hide();
    
private:
    static lv_obj_t *screen;
    static lv_display_t *display;
    static MachineConfig machines[MAX_MACHINES];
    
    // Edit mode state
    static bool edit_mode;
    static lv_obj_t *edit_mode_button;
    
    // Machine button widgets
    static lv_obj_t *machine_buttons[MAX_MACHINES];
    static lv_obj_t *edit_buttons[MAX_MACHINES];
    static lv_obj_t *move_up_buttons[MAX_MACHINES];
    static lv_obj_t *move_down_buttons[MAX_MACHINES];
    static lv_obj_t *delete_buttons[MAX_MACHINES];
    static lv_obj_t *add_button;  // Single add button
    
    // Configuration dialog
    static lv_obj_t *config_dialog;
    static lv_obj_t *dialog_content;
    static lv_obj_t *keyboard;
    static int editing_index;
    static lv_obj_t *ta_name;
    static lv_obj_t *ta_ssid;
    static lv_obj_t *ta_password;
    static lv_obj_t *ta_url;
    static lv_obj_t *ta_port;
    static lv_obj_t *dd_connection_type;
    
    // Delete confirmation dialog
    static lv_obj_t *delete_dialog;
    static int deleting_index;
    
    // Event handlers
    static void onMachineSelected(lv_event_t *e);
    static void onEditModeToggle(lv_event_t *e);
    static void onEditMachine(lv_event_t *e);
    static void onAddMachine(lv_event_t *e);
    static void onMoveUpMachine(lv_event_t *e);
    static void onMoveDownMachine(lv_event_t *e);
    static void onDeleteMachine(lv_event_t *e);
    static void onDeleteConfirm(lv_event_t *e);
    static void onDeleteCancel(lv_event_t *e);
    static void onConfigSave(lv_event_t *e);
    static void onConfigCancel(lv_event_t *e);
    static void onConnectionTypeChanged(lv_event_t *e);
    static void onTextareaFocused(lv_event_t *e);
    
    // Helper functions
    static void refreshMachineList();
    static void showConfigDialog(int index);
    static void hideConfigDialog();
    static void showDeleteConfirmDialog(int index);
    static void hideDeleteConfirmDialog();
    static void updateConnectionFields();
    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();
    static int getConfiguredMachineCount();
    static void swapMachines(int index1, int index2);
};

#endif // UI_MACHINE_SELECT_H
