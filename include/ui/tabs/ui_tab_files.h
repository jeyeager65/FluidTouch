#ifndef UI_TAB_FILES_H
#define UI_TAB_FILES_H

#include <lvgl.h>
#include <vector>
#include <string>

class UITabFiles {
public:
    static void create(lv_obj_t *tab);
    static void refreshFileList();
    static void refreshFileList(const std::string &path);  // Overload for specific path
    
private:
    static lv_obj_t *file_list_container;
    static lv_obj_t *status_label;
    static lv_obj_t *path_label;
    static lv_obj_t *storage_dropdown;
    static std::vector<std::string> file_names;
    static std::string current_path;  // Track current directory path
    static bool initial_load_done;    // Track if initial file list has been loaded
    
    static void refresh_button_event_cb(lv_event_t *e);
    static void file_button_event_cb(lv_event_t *e);
    static void storage_dropdown_event_cb(lv_event_t *e);
    static void up_button_event_cb(lv_event_t *e);
    static void parseFileList(const std::string &response);
    static void updateFileListUI();
    static std::string getParentPath(const std::string &path);
};

#endif // UI_TAB_FILES_H
