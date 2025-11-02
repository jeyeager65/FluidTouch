#include "ui/tabs/ui_tab_files.h"
#include "ui/ui_theme.h"
#include "network/fluidnc_client.h"
#include <Arduino.h>
#include <algorithm>
#include <ArduinoJson.h>

// Static member initialization
lv_obj_t *UITabFiles::file_list_container = nullptr;
lv_obj_t *UITabFiles::status_label = nullptr;
lv_obj_t *UITabFiles::path_label = nullptr;
lv_obj_t *UITabFiles::storage_dropdown = nullptr;
std::vector<std::string> UITabFiles::file_names;
std::string UITabFiles::current_path = "/sd/";  // Default to SD card root
bool UITabFiles::initial_load_done = false;     // Track initial load

// Structure to store file info with size (size=-1 means directory)
struct FileInfo {
    std::string name;
    int32_t size;  // Changed to int32_t to support -1 for directories
    bool is_directory;
};
static std::vector<FileInfo> file_list_with_sizes;

void UITabFiles::create(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tab, 10, 0);

    // Storage selection dropdown
    storage_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(storage_dropdown, "SD Card\nFlash");
    lv_dropdown_set_selected(storage_dropdown, 0);
    lv_obj_set_size(storage_dropdown, 150, 45);
    lv_obj_set_pos(storage_dropdown, 5, 5);
    lv_obj_set_style_text_font(storage_dropdown, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(storage_dropdown, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_text_color(storage_dropdown, lv_color_white(), 0);
    lv_obj_add_event_cb(storage_dropdown, storage_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    // Get the dropdown list and set its height
    lv_obj_t *dropdown_list = lv_dropdown_get_list(storage_dropdown);
    if (dropdown_list) {
        lv_obj_set_style_max_height(dropdown_list, 100, 0);
    }

    // Refresh button
    lv_obj_t *btn_refresh = lv_button_create(tab);
    lv_obj_set_size(btn_refresh, 120, 45);
    lv_obj_set_pos(btn_refresh, 165, 5);
    lv_obj_set_style_bg_color(btn_refresh, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_add_event_cb(btn_refresh, refresh_button_event_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_refresh = lv_label_create(btn_refresh);
    lv_label_set_text(lbl_refresh, LV_SYMBOL_REFRESH " Refresh");
    lv_obj_set_style_text_font(lbl_refresh, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_refresh);

    // Up button (navigate to parent directory)
    lv_obj_t *btn_up = lv_button_create(tab);
    lv_obj_set_size(btn_up, 100, 45);
    lv_obj_set_pos(btn_up, 295, 5);
    lv_obj_set_style_bg_color(btn_up, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_up, up_button_event_cb, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_up = lv_label_create(btn_up);
    lv_label_set_text(lbl_up, LV_SYMBOL_UP " Up");
    lv_obj_set_style_text_font(lbl_up, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_up);

    // Path label (shows current directory)
    path_label = lv_label_create(tab);
    lv_label_set_text(path_label, "/sd/");
    lv_obj_set_style_text_font(path_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(path_label, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(path_label, 415, 9);
    lv_label_set_long_mode(path_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(path_label, 250);

    // Status label
    status_label = lv_label_create(tab);
    lv_label_set_text(status_label, "Click Refresh");
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    lv_obj_set_pos(status_label, 415, 32);

    // File list container with scrolling
    file_list_container = lv_obj_create(tab);
    lv_obj_set_size(file_list_container, 770, 270);
    lv_obj_set_pos(file_list_container, 5, 65);
    lv_obj_set_style_bg_color(file_list_container, UITheme::BG_DARKER, LV_PART_MAIN);
    lv_obj_set_style_border_color(file_list_container, UITheme::BORDER_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(file_list_container, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(file_list_container, 5, LV_PART_MAIN);
    lv_obj_set_flex_flow(file_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(file_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(file_list_container, 6, 0);  // 6px spacing between file rows
    lv_obj_set_scroll_dir(file_list_container, LV_DIR_VER);
}

void UITabFiles::refreshFileList() {
    // Only auto-load once on first tab selection
    if (!initial_load_done) {
        initial_load_done = true;
        refreshFileList(current_path);
    }
}

void UITabFiles::refreshFileList(const std::string &path) {
    if (!FluidNCClient::isConnected()) {
        if (status_label) {
            lv_label_set_text(status_label, "Not connected");
            lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        }
        Serial.println("[Files] Not connected to FluidNC");
        return;
    }
    
    // Update current path
    current_path = path;
    
    // Update path label
    if (path_label) {
        lv_label_set_text(path_label, path.c_str());
    }
    
    if (status_label) {
        lv_label_set_text(status_label, "Loading...");
        lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
    }
    
    Serial.printf("[Files] Requesting file list for: %s\n", path.c_str());
    
    // Clear previous file list
    file_names.clear();
    
    // Register callback to receive JSON file list response
    FluidNCClient::setMessageCallback([](const char* message) {
        // Accumulate multi-line JSON responses
        static String jsonBuffer;
        static bool collecting = false;
        static uint32_t lastMessageTime = 0;
        
        String msg(message);
        msg.trim();  // Remove any leading/trailing whitespace
        uint32_t now = millis();
        
        // Reset buffer if it's been more than 3000ms since last message
        if (now - lastMessageTime > 3000) {
            if (jsonBuffer.length() > 0 && collecting) {
                Serial.printf("[Files] Timeout - parsing JSON buffer (%d bytes)\n", jsonBuffer.length());
                parseFileList(jsonBuffer.c_str());
                FluidNCClient::clearMessageCallback();
            }
            jsonBuffer = "";
            collecting = false;
        }
        lastMessageTime = now;
        
        // Skip status reports and GCode state messages
        if (msg.startsWith("<") || msg.startsWith("[GC:") || msg.startsWith("[MSG:")) {
            return;
        }
        
        // Skip PING messages during JSON collection
        if (msg.startsWith("PING:")) {
            return;
        }
        
        // Detect end of JSON response (ok line) - check this BEFORE printing/processing
        if (msg.equalsIgnoreCase("ok")) {
            if (collecting) {
                Serial.printf("[Files] Received 'ok', parsing %d bytes\n", jsonBuffer.length());
                parseFileList(jsonBuffer.c_str());
                jsonBuffer = "";
                collecting = false;
                FluidNCClient::clearMessageCallback();
            }
            return;  // Always return early for "ok" messages
        }
        
        Serial.printf("[Files] Received line: %s\n", msg.c_str());
        
        // Start collecting when we see JSON start (either [JSON: wrapper or raw JSON with {"files")
        if (msg.startsWith("[JSON:") || msg.startsWith("{\"files")) {
            if (!collecting) {
                Serial.println("[Files] Starting to collect JSON response");
                collecting = true;
                jsonBuffer = "";
            }
            // Remove [JSON: prefix and ] suffix if present
            String jsonLine = msg;
            if (jsonLine.startsWith("[JSON:")) {
                jsonLine.replace("[JSON:", "");
                if (jsonLine.endsWith("]")) {
                    jsonLine.remove(jsonLine.length() - 1);
                }
            }
            jsonBuffer += jsonLine;
            Serial.printf("[Files] JSON buffer now: %d bytes\n", jsonBuffer.length());
        } else if (collecting) {
            // Continue collecting any other lines while in collection mode
            jsonBuffer += msg;
            Serial.printf("[Files] JSON buffer now: %d bytes\n", jsonBuffer.length());
        }
    });
    
    // Send JSON file list command for specified path
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "$Files/ListGcode=%s\n", path.c_str());
    FluidNCClient::sendCommand(cmd);
}

void UITabFiles::refresh_button_event_cb(lv_event_t *e) {
    Serial.println("[Files] Refresh button clicked");
    refreshFileList();
}

void UITabFiles::storage_dropdown_event_cb(lv_event_t *e) {
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    Serial.printf("[Files] Storage changed to index: %d\n", selected);
    
    // Switch storage root path
    if (selected == 0) {
        // SD Card
        current_path = "/sd/";
        Serial.println("[Files] Switched to SD Card");
    } else if (selected == 1) {
        // Flash
        current_path = "/localfs/";
        Serial.println("[Files] Switched to Flash");
    }
    
    // Update path label and refresh file list
    if (path_label) {
        lv_label_set_text(path_label, current_path.c_str());
    }
    refreshFileList(current_path);
}

void UITabFiles::up_button_event_cb(lv_event_t *e) {
    Serial.printf("[Files] Up button clicked, current path: %s\n", current_path.c_str());
    
    // Don't go above storage root (/sd/ or /localfs/)
    if (current_path == "/sd/" || current_path == "/sd" || 
        current_path == "/localfs/" || current_path == "/localfs") {
        Serial.println("[Files] Already at root, cannot go up");
        if (status_label) {
            lv_label_set_text(status_label, "At root");
            lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        }
        return;
    }
    
    std::string parent = getParentPath(current_path);
    Serial.printf("[Files] Navigating to parent: %s\n", parent.c_str());
    refreshFileList(parent);
}

std::string UITabFiles::getParentPath(const std::string &path) {
    // Remove trailing slash if present
    std::string p = path;
    if (p.length() > 1 && p[p.length() - 1] == '/') {
        p = p.substr(0, p.length() - 1);
    }
    
    // Find last slash
    size_t lastSlash = p.find_last_of('/');
    if (lastSlash == std::string::npos || lastSlash == 0) {
        return "/sd/";  // Return root
    }
    
    // Return path up to (but not including) the last slash to avoid double slashes
    std::string parent = p.substr(0, lastSlash);
    
    // Remove trailing slash from parent if present (except for root)
    if (parent.length() > 1 && parent[parent.length() - 1] == '/') {
        parent = parent.substr(0, parent.length() - 1);
    }
    
    return parent;
}

void UITabFiles::file_button_event_cb(lv_event_t *e) {
    // Not used anymore - individual buttons for Play/Delete
}

static void directory_button_event_cb(lv_event_t *e) {
    const char *dirname = (const char*)lv_event_get_user_data(e);
    if (dirname) {
        Serial.printf("[Files] Opening directory: %s\n", dirname);
        UITabFiles::refreshFileList(dirname);
    }
}

static void play_button_event_cb(lv_event_t *e) {
    const char *filename = (const char*)lv_event_get_user_data(e);
    if (filename) {
        Serial.printf("[Files] Play file: %s\n", filename);
        
        // Determine command prefix based on file path
        const char *cmd_prefix;
        if (strncmp(filename, "/localfs/", 9) == 0) {
            cmd_prefix = "$LocalFS/Run=";
        } else {
            cmd_prefix = "$SD/Run=";
        }
        
        // Send run command to FluidNC
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s%s\n", cmd_prefix, filename);
        FluidNCClient::sendCommand(cmd);
    }
}

static void delete_confirm_event_cb(lv_event_t *e) {
    const char *filename = (const char*)lv_event_get_user_data(e);
    
    if (filename) {
        Serial.printf("[Files] Deleting file: %s\n", filename);
        
        // Determine command prefix based on file path
        const char *cmd_prefix;
        if (strncmp(filename, "/localfs/", 9) == 0) {
            cmd_prefix = "$LocalFS/Delete=";
        } else {
            cmd_prefix = "$SD/Delete=";
        }
        
        // Send delete command to FluidNC
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s%s\n", cmd_prefix, filename);
        FluidNCClient::sendCommand(cmd);
    }
    
    // Close the dialog
    lv_obj_t *dialog = (lv_obj_t*)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));
    if (dialog) {
        lv_obj_del(dialog);
    }
}

static void delete_cancel_event_cb(lv_event_t *e) {
    Serial.println("[Files] Delete cancelled");
    
    // Close the dialog
    lv_obj_t *dialog = (lv_obj_t*)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));
    if (dialog) {
        lv_obj_del(dialog);
    }
}

static void delete_button_event_cb(lv_event_t *e) {
    const char *filename = (const char*)lv_event_get_user_data(e);
    if (filename) {
        Serial.printf("[Files] Delete requested for: %s\n", filename);
        
        // Create modal background
        lv_obj_t *dialog = lv_obj_create(lv_scr_act());
        lv_obj_set_size(dialog, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(dialog, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_opa(dialog, LV_OPA_70, 0);
        lv_obj_set_style_border_width(dialog, 0, 0);
        lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
        
        // Dialog content box
        lv_obj_t *content = lv_obj_create(dialog);
        lv_obj_set_size(content, 500, 220);
        lv_obj_center(content);
        lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
        lv_obj_set_style_border_color(content, UITheme::STATE_ALARM, 0);
        lv_obj_set_style_border_width(content, 3, 0);
        lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(content, 20, 0);
        lv_obj_set_style_pad_gap(content, 15, 0);
        lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
        
        // Warning icon and title
        lv_obj_t *title = lv_label_create(content);
        lv_label_set_text_fmt(title, "%s Delete File?", LV_SYMBOL_WARNING);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
        
        // File name
        lv_obj_t *name_label = lv_label_create(content);
        lv_label_set_text_fmt(name_label, "%s", filename);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(name_label, UITheme::TEXT_LIGHT, 0);
        lv_obj_set_style_text_align(name_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_label, 450);
        
        // Message
        lv_obj_t *msg_label = lv_label_create(content);
        lv_label_set_text(msg_label, "This action cannot be undone.");
        lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(msg_label, UITheme::UI_WARNING, 0);
        
        // Button container
        lv_obj_t *btn_container = lv_obj_create(content);
        lv_obj_set_size(btn_container, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn_container, 0, 0);
        lv_obj_set_style_pad_all(btn_container, 0, 0);
        lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
        
        // Store filename in dialog user data (use static storage)
        static char filename_storage[10][128];  // Support up to 10 concurrent dialogs
        static int storage_index = 0;
        strncpy(filename_storage[storage_index], filename, 127);
        filename_storage[storage_index][127] = '\0';
        
        // Cancel button
        lv_obj_t *cancel_btn = lv_btn_create(btn_container);
        lv_obj_set_size(cancel_btn, 180, 50);
        lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
        lv_obj_set_user_data(cancel_btn, dialog);  // Store dialog reference
        lv_obj_add_event_cb(cancel_btn, delete_cancel_event_cb, LV_EVENT_CLICKED, nullptr);
        
        lv_obj_t *cancel_label = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_label, "Cancel");
        lv_obj_set_style_text_font(cancel_label, &lv_font_montserrat_18, 0);
        lv_obj_center(cancel_label);
        
        // Delete button
        lv_obj_t *delete_btn = lv_btn_create(btn_container);
        lv_obj_set_size(delete_btn, 180, 50);
        lv_obj_set_style_bg_color(delete_btn, UITheme::STATE_ALARM, 0);
        lv_obj_set_user_data(delete_btn, dialog);  // Store dialog reference
        lv_obj_add_event_cb(delete_btn, delete_confirm_event_cb, LV_EVENT_CLICKED, filename_storage[storage_index]);
        
        lv_obj_t *delete_label = lv_label_create(delete_btn);
        lv_label_set_text(delete_label, LV_SYMBOL_TRASH " Delete");
        lv_obj_set_style_text_font(delete_label, &lv_font_montserrat_18, 0);
        lv_obj_center(delete_label);
        
        storage_index = (storage_index + 1) % 10;
    }
}

void UITabFiles::parseFileList(const std::string &response) {
    // Parse FluidNC $Files/ListGcode JSON response
    // Expected format: {"files":[{"name":"file.gcode","size":12345},...],"path":"/sd/"}
    file_names.clear();
    file_list_with_sizes.clear();
    
    Serial.printf("[Files] Parsing JSON file list, response length: %d\n", response.length());
    Serial.printf("[Files] JSON: %s\n", response.c_str());
    
    // Parse JSON
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (error) {
        Serial.printf("[Files] JSON parsing failed: %s\n", error.c_str());
        if (status_label) {
            lv_label_set_text(status_label, "Error parsing file list");
            lv_obj_set_style_text_color(status_label, UITheme::STATE_ALARM, 0);
        }
        return;
    }
    
    // Check for error field in response
    if (doc["error"].is<const char*>()) {
        const char* error_msg = doc["error"];
        Serial.printf("[Files] FluidNC error: %s\n", error_msg);
        if (status_label) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Error: %s", error_msg);
            lv_label_set_text(status_label, msg);
            lv_obj_set_style_text_color(status_label, UITheme::STATE_ALARM, 0);
        }
        updateFileListUI();
        return;
    }
    
    // Check if response has files array
    if (!doc["files"].is<JsonArray>()) {
        Serial.println("[Files] No 'files' array in JSON response");
        if (status_label) {
            lv_label_set_text(status_label, "No files found");
            lv_obj_set_style_text_color(status_label, UITheme::UI_INFO, 0);
        }
        updateFileListUI();
        return;
    }
    
    JsonArray files = doc["files"];
    
    // Extract file and directory information
    for (JsonObject file : files) {
        if (file["name"].is<const char*>()) {
            const char* filename = file["name"];
            int32_t filesize = 0;
            
            // Size can be either string or integer
            if (file["size"].is<const char*>()) {
                // Parse string to integer
                const char* size_str = file["size"];
                filesize = atoi(size_str);
            } else if (file["size"].is<int>()) {
                filesize = file["size"];
            } else {
                Serial.printf("[Files] Warning: Could not parse size for '%s'\n", filename);
                continue;
            }
            
            bool is_dir = (filesize == -1);
            
            file_names.push_back(filename);
            file_list_with_sizes.push_back({filename, filesize, is_dir});
            Serial.printf("[Files] Added %s: '%s' (%d bytes)\n", 
                is_dir ? "directory" : "file", filename, filesize);
        }
    }
    
    // Sort: directories first, then files, both alphabetically (case-insensitive)
    std::sort(file_list_with_sizes.begin(), file_list_with_sizes.end(), 
        [](const FileInfo &a, const FileInfo &b) {
            // Directories come before files
            if (a.is_directory != b.is_directory) {
                return a.is_directory;  // true (directory) sorts before false (file)
            }
            // Within same type, sort alphabetically (case-insensitive)
            std::string a_lower = a.name;
            std::string b_lower = b.name;
            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
            return a_lower < b_lower;
        });
    
    Serial.printf("[Files] Parsed %d files from JSON\n", file_list_with_sizes.size());
    
    updateFileListUI();
}

void UITabFiles::updateFileListUI() {
    if (!file_list_container) return;
    
    // Clear existing file buttons
    lv_obj_clean(file_list_container);
    
    if (file_list_with_sizes.empty()) {
        lv_obj_t *empty_label = lv_label_create(file_list_container);
        lv_label_set_text(empty_label, "No files found");
        lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(empty_label, UITheme::TEXT_MEDIUM, 0);
        
        if (status_label) {
            lv_label_set_text(status_label, "No files on SD card");
            lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        }
        return;
    }
    
    // Static storage for filenames/paths (persistent across callbacks)
    static char filenames_storage[100][256];  // Increased size for full paths
    size_t max_files = std::min(file_list_with_sizes.size(), (size_t)100);
    
    // Create file/directory entries
    for (size_t i = 0; i < max_files; i++) {
        const FileInfo &file = file_list_with_sizes[i];
        
        // Build full path for the entry
        std::string full_path = current_path;
        if (full_path[full_path.length() - 1] != '/') {
            full_path += "/";
        }
        full_path += file.name;
        // Note: Don't add trailing slash for directories - FluidNC doesn't like it
        
        // Store full path in persistent storage
        strncpy(filenames_storage[i], full_path.c_str(), 255);
        filenames_storage[i][255] = '\0';
        
        // File/directory row container
        lv_obj_t *file_row = lv_obj_create(file_list_container);
        lv_obj_set_size(file_row, 750, 46);
        lv_obj_set_style_bg_color(file_row, file.is_directory ? UITheme::BG_BUTTON : UITheme::BG_DARKER, 0);
        lv_obj_set_style_border_width(file_row, 1, 0);
        lv_obj_set_style_border_color(file_row, UITheme::BORDER_MEDIUM, 0);
        lv_obj_set_style_pad_all(file_row, 5, 0);
        lv_obj_set_style_radius(file_row, 3, 0);
        lv_obj_clear_flag(file_row, LV_OBJ_FLAG_SCROLLABLE);
        
        if (file.is_directory) {
            // Make directory row clickable
            lv_obj_add_flag(file_row, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(file_row, directory_button_event_cb, LV_EVENT_CLICKED, filenames_storage[i]);
        } else {
            lv_obj_clear_flag(file_row, LV_OBJ_FLAG_CLICKABLE);
        }
        
        // Icon + Filename label (left side)
        lv_obj_t *lbl_filename = lv_label_create(file_row);
        if (file.is_directory) {
            char label_text[256];
            snprintf(label_text, sizeof(label_text), LV_SYMBOL_DIRECTORY " /%s", file.name.c_str());
            lv_label_set_text(lbl_filename, label_text);
            lv_obj_set_style_text_color(lbl_filename, UITheme::ACCENT_SECONDARY, 0);
        } else {
            lv_label_set_text(lbl_filename, file.name.c_str());
            lv_obj_set_style_text_color(lbl_filename, lv_color_white(), 0);
        }
        lv_obj_set_style_text_font(lbl_filename, &lv_font_montserrat_20, 0);
        lv_obj_align(lbl_filename, LV_ALIGN_LEFT_MID, 5, 0);
        lv_label_set_long_mode(lbl_filename, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl_filename, file.is_directory ? 720 : 400);
        
        // Only show size and buttons for files, not directories
        if (!file.is_directory) {
            // File size label (center)
            lv_obj_t *lbl_size = lv_label_create(file_row);
            char size_str[32];
            if (file.size >= 1024 * 1024) {
                snprintf(size_str, sizeof(size_str), "%.2f MB", file.size / (1024.0f * 1024.0f));
            } else if (file.size >= 1024) {
                snprintf(size_str, sizeof(size_str), "%.1f KB", file.size / 1024.0f);
            } else {
                snprintf(size_str, sizeof(size_str), "%d B", file.size);
            }
            lv_label_set_text(lbl_size, size_str);
            lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(lbl_size, UITheme::TEXT_MEDIUM, 0);
            lv_obj_align(lbl_size, LV_ALIGN_LEFT_MID, 420, 0);
            
            // Delete button
            lv_obj_t *btn_delete = lv_button_create(file_row);
            lv_obj_set_size(btn_delete, 70, 38);
            lv_obj_align(btn_delete, LV_ALIGN_RIGHT_MID, -80, 0);
            lv_obj_set_style_bg_color(btn_delete, UITheme::BTN_ESTOP, 0);
            lv_obj_set_style_radius(btn_delete, 3, 0);
            lv_obj_add_event_cb(btn_delete, delete_button_event_cb, LV_EVENT_CLICKED, filenames_storage[i]);
            
            lv_obj_t *lbl_delete = lv_label_create(btn_delete);
            lv_label_set_text(lbl_delete, LV_SYMBOL_TRASH);
            lv_obj_set_style_text_font(lbl_delete, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_delete);
            
            // Play button
            lv_obj_t *btn_play = lv_button_create(file_row);
            lv_obj_set_size(btn_play, 70, 38);
            lv_obj_align(btn_play, LV_ALIGN_RIGHT_MID, -5, 0);
            lv_obj_set_style_bg_color(btn_play, UITheme::BTN_PLAY, 0);
            lv_obj_set_style_radius(btn_play, 3, 0);
            lv_obj_add_event_cb(btn_play, play_button_event_cb, LV_EVENT_CLICKED, filenames_storage[i]);
            
            lv_obj_t *lbl_play = lv_label_create(btn_play);
            lv_label_set_text(lbl_play, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_font(lbl_play, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_play);
        }  // End of if (!file.is_directory)
    }  // End of for loop
    
    if (status_label) {
        char buf[64];
        int file_count = 0;
        int dir_count = 0;
        for (const auto &f : file_list_with_sizes) {
            if (f.is_directory) dir_count++;
            else file_count++;
        }
        if (dir_count > 0 && file_count > 0) {
            snprintf(buf, sizeof(buf), "%d folder(s), %d file(s)", dir_count, file_count);
        } else if (dir_count > 0) {
            snprintf(buf, sizeof(buf), "%d folder(s)", dir_count);
        } else {
            snprintf(buf, sizeof(buf), "%d file(s)", file_count);
        }
        lv_label_set_text(status_label, buf);
        lv_obj_set_style_text_color(status_label, UITheme::UI_SUCCESS, 0);
    }
}
