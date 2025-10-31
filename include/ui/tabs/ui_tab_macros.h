#ifndef UI_TAB_MACROS_H
#define UI_TAB_MACROS_H

#include <lvgl.h>
#include <vector>
#include <string>

#define MAX_MACROS 9

// Macro configuration structure
struct MacroConfig {
    char name[32];
    char file_path[128];  // Path relative to /fluidtouch/macros/
    uint8_t color_index;  // 0-7 for MACRO_COLOR_1 through MACRO_COLOR_8
    bool is_configured;
};

class UITabMacros {
public:
    static void create(lv_obj_t *tab);
    
    // Progress display update (public for main loop to call)
    static void updateProgress(int percent, const char* macro_name, const char* message);
    static void showProgress();
    static void hideProgress();
    
private:
    static lv_obj_t *parent_tab;
    static lv_obj_t *macro_container;
    static lv_obj_t *btn_edit;
    static lv_obj_t *btn_add;
    static lv_obj_t *btn_done;
    
    // Progress display (normal mode)
    static lv_obj_t *progress_container;
    static lv_obj_t *lbl_macro_name;
    static lv_obj_t *bar_progress;
    static lv_obj_t *lbl_percent;
    static lv_obj_t *lbl_message;
    static char running_macro_name[32];  // Store the name of currently running macro
    
    static bool is_edit_mode;
    static MacroConfig macros[MAX_MACROS];
    
    // Macro buttons
    static lv_obj_t *macro_buttons[MAX_MACROS];
    static lv_obj_t *edit_buttons[MAX_MACROS];
    static lv_obj_t *up_buttons[MAX_MACROS];
    static lv_obj_t *down_buttons[MAX_MACROS];
    static lv_obj_t *delete_buttons[MAX_MACROS];
    
    // Config dialog
    static lv_obj_t *config_dialog;
    static lv_obj_t *config_name_textarea;
    static lv_obj_t *config_path_dropdown;
    static lv_obj_t *config_color_buttons[8];
    static int selected_color_index;
    static lv_obj_t *keyboard;
    static int editing_index;
    static std::vector<std::string> macro_files;
    
    // Delete confirmation dialog
    static lv_obj_t *delete_dialog;
    
    // Helper functions
    static void refreshMacroList();
    static void loadMacros();
    static void saveMacros();
    static int getConfiguredMacroCount();
    static void swapMacros(int index1, int index2);
    static lv_color_t getColorByIndex(int index);
    static bool findPreviousConfiguredIndex(int current_index);
    static bool findNextConfiguredIndex(int current_index);
    static void loadMacroFilesFromSD();
    
    // Event handlers
    static void onEditModeToggle(lv_event_t *e);
    static void onMacroClicked(lv_event_t *e);
    static void onAddMacro(lv_event_t *e);
    static void onEditMacro(lv_event_t *e);
    static void onDeleteMacro(lv_event_t *e);
    static void onMoveUpMacro(lv_event_t *e);
    static void onMoveDownMacro(lv_event_t *e);
    static void onRefreshFiles(lv_event_t *e);
    
    // Config dialog
    static void showConfigDialog(bool is_add);
    static void hideConfigDialog();
    static void onConfigSave(lv_event_t *e);
    static void onConfigCancel(lv_event_t *e);
    static void onColorButtonClicked(lv_event_t *e);
    static void onTextareaFocused(lv_event_t *e);
    static void showKeyboard(lv_obj_t *ta);
    static void hideKeyboard();
    
    // Delete confirmation
    static void showDeleteConfirmDialog();
    static void hideDeleteConfirmDialog();
    static void onDeleteConfirm(lv_event_t *e);
    static void onDeleteCancel(lv_event_t *e);
};

#endif // UI_TAB_MACROS_H
