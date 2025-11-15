#ifndef UI_TAB_FILES_H
#define UI_TAB_FILES_H

#include <lvgl.h>
#include <vector>
#include <string>

enum class StorageSource {
    FLUIDNC_SD = 0,
    FLUIDNC_FLASH = 1,
    DISPLAY_SD = 2
};

class UITabFiles {
public:
    static void create(lv_obj_t *tab);
    static void refreshFileList();
    static void refreshFileList(const std::string &path);  // Overload for specific path
    static void listDisplaySDFiles(const std::string &path);
    static void requestRefresh();  // Request a refresh (called from callbacks)
    static void checkPendingRefresh();  // Check and execute pending refresh (called from main loop)
    static StorageSource current_storage;
    
    // Cache for each storage source
    struct FileInfo {
        std::string name;
        int32_t size;
        bool is_directory;
    };
    
    struct StorageCache {
        std::string cached_path;
        bool is_cached;
        std::vector<FileInfo> file_list;
    };
    static StorageCache fluidnc_sd_cache;
    static StorageCache fluidnc_flash_cache;
    static StorageCache display_sd_cache;
    
private:
    static lv_obj_t *file_list_container;
    static lv_obj_t *status_label;
    static lv_obj_t *path_label;
    static lv_obj_t *storage_dropdown;
    static lv_obj_t *upload_dialog;
    static lv_obj_t *upload_progress_dialog;
    static lv_obj_t *upload_progress_bar;
    static lv_obj_t *upload_progress_label;
    static std::vector<std::string> file_names;
    static std::string current_path;  // Track current directory path
    static bool initial_load_done;    // Track if initial file list has been loaded
    static bool refresh_pending;      // Flag to request refresh from callbacks
    
    static void refresh_button_event_cb(lv_event_t *e);
    static void storage_dropdown_event_cb(lv_event_t *e);
    static void up_button_event_cb(lv_event_t *e);
    static void upload_button_event_cb(lv_event_t *e);
    static void parseFileList(const std::string &response);
    static void updateFileListUI();
    static std::string getParentPath(const std::string &path);
    static void showUploadDialog(const char* filename, const char* fullPath, size_t fileSize);
    static void showUploadProgress(const char* filename);
    static void updateUploadProgress(size_t current, size_t total);
    static void closeUploadProgress(bool success, const char* error);
    static bool isDisplaySDAvailable();  // Check if Display SD card is available
};

#endif // UI_TAB_FILES_H
