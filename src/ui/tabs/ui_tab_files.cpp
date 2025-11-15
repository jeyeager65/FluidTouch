#include "ui/tabs/ui_tab_files.h"
#include "ui/ui_theme.h"
#include "ui/upload_manager.h"
#include "network/fluidnc_client.h"
#include "config.h"
#include <Arduino.h>
#include <algorithm>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>

// Static member initialization
lv_obj_t *UITabFiles::file_list_container = nullptr;
lv_obj_t *UITabFiles::status_label = nullptr;
lv_obj_t *UITabFiles::path_label = nullptr;
lv_obj_t *UITabFiles::storage_dropdown = nullptr;
lv_obj_t *UITabFiles::upload_dialog = nullptr;
lv_obj_t *UITabFiles::upload_progress_dialog = nullptr;
lv_obj_t *UITabFiles::upload_progress_bar = nullptr;
lv_obj_t *UITabFiles::upload_progress_label = nullptr;
std::vector<std::string> UITabFiles::file_names;
std::string UITabFiles::current_path = "/sd/";  // Default to SD card root
bool UITabFiles::initial_load_done = false;     // Track initial load
bool UITabFiles::refresh_pending = false;       // Track pending refresh request
StorageSource UITabFiles::current_storage = StorageSource::FLUIDNC_SD;

// Cache for each storage source
UITabFiles::StorageCache UITabFiles::fluidnc_sd_cache = {"", false, {}};
UITabFiles::StorageCache UITabFiles::fluidnc_flash_cache = {"", false, {}};
UITabFiles::StorageCache UITabFiles::display_sd_cache = {"", false, {}};

// Helper to get current storage cache
static UITabFiles::StorageCache* getCurrentCache() {
    if (UITabFiles::current_storage == StorageSource::FLUIDNC_SD) {
        return &UITabFiles::fluidnc_sd_cache;
    } else if (UITabFiles::current_storage == StorageSource::FLUIDNC_FLASH) {
        return &UITabFiles::fluidnc_flash_cache;
    } else {
        return &UITabFiles::display_sd_cache;
    }
}

// Check if Display SD card is available
bool UITabFiles::isDisplaySDAvailable() {
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[Files] No SD card detected");
        return false;
    }
    return true;
}

// Request a refresh (called from callbacks)
void UITabFiles::requestRefresh() {
    refresh_pending = true;
}

// Check and execute pending refresh (called from main loop)
void UITabFiles::checkPendingRefresh() {
    if (!refresh_pending) return;
    
    refresh_pending = false;
    
    // Clear cache for current storage
    if (current_storage == StorageSource::FLUIDNC_SD) {
        fluidnc_sd_cache.is_cached = false;
    } else if (current_storage == StorageSource::FLUIDNC_FLASH) {
        fluidnc_flash_cache.is_cached = false;
    } else if (current_storage == StorageSource::DISPLAY_SD) {
        display_sd_cache.is_cached = false;
    }
    
    // Refresh the file list
    if (current_storage == StorageSource::DISPLAY_SD) {
        listDisplaySDFiles(current_path);
    } else {
        refreshFileList(current_path);
    }
}

void UITabFiles::create(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tab, 10, 0);

    // Storage selection dropdown
    storage_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(storage_dropdown, "FluidNC SD\nFluidNC Flash\nDisplay SD");
    lv_dropdown_set_selected(storage_dropdown, 0);
    lv_obj_set_size(storage_dropdown, 150, 45);
    lv_obj_set_pos(storage_dropdown, 5, 5);
    lv_obj_set_style_text_font(storage_dropdown, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(storage_dropdown, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_text_color(storage_dropdown, lv_color_white(), 0);
    lv_obj_add_event_cb(storage_dropdown, storage_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    
    // Get the dropdown list and set its height to show all 3 options without scrolling
    lv_obj_t *dropdown_list = lv_dropdown_get_list(storage_dropdown);
    if (dropdown_list) {
        lv_obj_set_style_max_height(dropdown_list, 150, 0);  // 3 items × ~48px each
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
    
    // Check if machine is in IDLE state - don't fetch files if machine is running
    const FluidNCStatus& status = FluidNCClient::getStatus();
    if (status.state != STATE_IDLE) {
        if (status_label) {
            lv_label_set_text(status_label, "Machine must be IDLE to list files");
            lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        }
        Serial.printf("[Files] Machine not in IDLE state (state=%d), skipping file list\n", status.state);
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
    
    // Clear cache for current storage
    if (current_storage == StorageSource::FLUIDNC_SD) {
        fluidnc_sd_cache.is_cached = false;
    } else if (current_storage == StorageSource::FLUIDNC_FLASH) {
        fluidnc_flash_cache.is_cached = false;
    } else if (current_storage == StorageSource::DISPLAY_SD) {
        display_sd_cache.is_cached = false;
    }
    
    // Use appropriate refresh function based on storage type
    if (current_storage == StorageSource::DISPLAY_SD) {
        listDisplaySDFiles(current_path);
    } else {
        refreshFileList(current_path);  // Always refresh with current path, not just on initial load
    }
}

void UITabFiles::storage_dropdown_event_cb(lv_event_t *e) {
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    Serial.printf("[Files] Storage changed to index: %d\n", selected);
    
    current_storage = static_cast<StorageSource>(selected);
    
    // Switch storage root path
    if (current_storage == StorageSource::FLUIDNC_SD) {
        // FluidNC SD Card
        Serial.println("[Files] Switched to FluidNC SD Card");
        
        // Check if already cached - restore cached path
        if (fluidnc_sd_cache.is_cached) {
            current_path = fluidnc_sd_cache.cached_path;
            Serial.printf("[Files] Using cached FluidNC SD file list (%d items) at %s\n", fluidnc_sd_cache.file_list.size(), current_path.c_str());
            if (path_label) {
                lv_label_set_text(path_label, current_path.c_str());
            }
            updateFileListUI();  // Display the cached data (will update status_label with counts)
            return;
        }
        
        // Not cached - start at root
        current_path = "/sd/";
        refreshFileList(current_path);
    } else if (current_storage == StorageSource::FLUIDNC_FLASH) {
        // FluidNC Flash
        Serial.println("[Files] Switched to FluidNC Flash");
        
        // Check if already cached - restore cached path
        if (fluidnc_flash_cache.is_cached) {
            current_path = fluidnc_flash_cache.cached_path;
            Serial.printf("[Files] Using cached FluidNC Flash file list (%d items) at %s\n", fluidnc_flash_cache.file_list.size(), current_path.c_str());
            if (path_label) {
                lv_label_set_text(path_label, current_path.c_str());
            }
            updateFileListUI();  // Display the cached data (will update status_label with counts)
            return;
        }
        
        // Not cached - start at root
        current_path = "/localfs/";
        refreshFileList(current_path);
    } else if (current_storage == StorageSource::DISPLAY_SD) {
        // Display SD Card
        Serial.println("[Files] Switched to Display SD Card");
        
        // Check if already cached - restore cached path
        if (display_sd_cache.is_cached) {
            // Verify SD card is still available before using cache
            if (!isDisplaySDAvailable()) {
                Serial.println("[Files] SD card no longer available, invalidating cache");
                display_sd_cache.is_cached = false;
                display_sd_cache.file_list.clear();
                if (status_label) {
                    lv_label_set_text(status_label, "SD card not available");
                    lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
                }
                if (path_label) {
                    lv_label_set_text(path_label, "/");
                }
                // Clear file list UI
                if (file_list_container) {
                    lv_obj_clean(file_list_container);
                }
                return;
            }
            
            current_path = display_sd_cache.cached_path;
            Serial.printf("[Files] Using cached Display SD file list (%d items) at %s\n", display_sd_cache.file_list.size(), current_path.c_str());
            if (path_label) {
                lv_label_set_text(path_label, current_path.c_str());
            }
            updateFileListUI();  // Display the cached data (will update status_label with counts)
            return;
        }
        
        // Not cached - start at root and initialize SD card if not already done
        current_path = "/";
        Serial.println("[Files] Attempting to initialize SD card...");
        bool sdInitialized = false;
        
        // Wrap SD initialization in try-catch equivalent (check for crashes)
        sdInitialized = UploadManager::init();
        
        if (!sdInitialized) {
            Serial.println("[Files] Display SD card initialization failed");
            if (status_label) {
                lv_label_set_text(status_label, "SD card not available");
                lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
            }
            if (path_label) {
                lv_label_set_text(path_label, "/");
            }
            // Clear file list UI
            if (file_list_container) {
                lv_obj_clean(file_list_container);
            }
            return;
        }
        
        Serial.println("[Files] SD card initialized successfully");
        listDisplaySDFiles(current_path);
    }
    
    // Update path label
    if (path_label) {
        lv_label_set_text(path_label, current_path.c_str());
    }
}

void UITabFiles::up_button_event_cb(lv_event_t *e) {
    Serial.printf("[Files] Up button clicked, current path: %s\n", current_path.c_str());
    
    // Don't go above storage root (/sd/ or /localfs/ or /)
    if (current_path == "/sd/" || current_path == "/sd" || 
        current_path == "/localfs/" || current_path == "/localfs" ||
        current_path == "/") {
        Serial.println("[Files] Already at root, cannot go up");
        if (status_label) {
            lv_label_set_text(status_label, "At root");
            lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        }
        return;
    }
    
    std::string parent = getParentPath(current_path);
    Serial.printf("[Files] Navigating to parent: %s\n", parent.c_str());
    
    // Invalidate cache for current storage when navigating
    if (current_storage == StorageSource::FLUIDNC_SD) {
        fluidnc_sd_cache.is_cached = false;
    } else if (current_storage == StorageSource::FLUIDNC_FLASH) {
        fluidnc_flash_cache.is_cached = false;
    } else if (current_storage == StorageSource::DISPLAY_SD) {
        display_sd_cache.is_cached = false;
    }
    
    // Use appropriate function based on storage type
    if (current_storage == StorageSource::DISPLAY_SD) {
        listDisplaySDFiles(parent);
    } else {
        refreshFileList(parent);
    }
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
        // Return appropriate root based on current storage
        if (current_storage == StorageSource::FLUIDNC_FLASH) {
            return "/localfs/";
        } else if (current_storage == StorageSource::DISPLAY_SD) {
            return "/";
        } else {
            return "/sd/";
        }
    }
    
    // Return path up to (but not including) the last slash to avoid double slashes
    std::string parent = p.substr(0, lastSlash);
    
    // Remove trailing slash from parent if present (except for root)
    if (parent.length() > 1 && parent[parent.length() - 1] == '/') {
        parent = parent.substr(0, parent.length() - 1);
    }
    
    return parent;
}

static void directory_button_event_cb(lv_event_t *e) {
    const char *dirname = (const char*)lv_event_get_user_data(e);
    if (dirname) {
        Serial.printf("[Files] Opening directory: %s\n", dirname);
        
        // Invalidate cache for current storage when navigating
        if (UITabFiles::current_storage == StorageSource::FLUIDNC_SD) {
            UITabFiles::fluidnc_sd_cache.is_cached = false;
        } else if (UITabFiles::current_storage == StorageSource::FLUIDNC_FLASH) {
            UITabFiles::fluidnc_flash_cache.is_cached = false;
        } else if (UITabFiles::current_storage == StorageSource::DISPLAY_SD) {
            UITabFiles::display_sd_cache.is_cached = false;
        }
        
        // Handle Display SD navigation differently
        if (UITabFiles::current_storage == StorageSource::DISPLAY_SD) {
            UITabFiles::listDisplaySDFiles(dirname);
        } else {
            UITabFiles::refreshFileList(dirname);
        }
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
        
        // Request a refresh after a short delay (500ms)
        // The refresh will be handled by checkPendingRefresh() in the main loop
        lv_timer_create([](lv_timer_t *timer) {
            UITabFiles::requestRefresh();
            lv_timer_delete(timer);
        }, 500, nullptr);
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
    
    // Get current storage cache
    StorageCache* cache = getCurrentCache();
    cache->file_list.clear();
    
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
            cache->file_list.push_back({filename, filesize, is_dir});
            Serial.printf("[Files] Added %s: '%s' (%d bytes)\n", 
                is_dir ? "directory" : "file", filename, filesize);
        }
    }
    
    // Load folders_on_top preference
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);  // Read-only
    bool folders_on_top = prefs.getBool("folders_on_top", false);  // Default to false (folders at bottom)
    prefs.end();
    
    // Sort based on user preference
    if (folders_on_top) {
        // Folders first (at top), then files, both alphabetically (case-insensitive)
        std::sort(cache->file_list.begin(), cache->file_list.end(), 
            [](const FileInfo &a, const FileInfo &b) {
                // Directories come before files (directories at top)
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
    } else {
        // Files first, then directories at bottom, both alphabetically (case-insensitive)
        std::sort(cache->file_list.begin(), cache->file_list.end(), 
            [](const FileInfo &a, const FileInfo &b) {
                // Files come before directories (directories at bottom)
                if (a.is_directory != b.is_directory) {
                    return !a.is_directory;  // false (file) sorts before true (directory)
                }
                // Within same type, sort alphabetically (case-insensitive)
                std::string a_lower = a.name;
                std::string b_lower = b.name;
                std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
                std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
                return a_lower < b_lower;
            });
    }
    
    Serial.printf("[Files] Parsed %d files from JSON (folders_on_top=%d)\n", cache->file_list.size(), folders_on_top);
    
    // Mark cache as valid after successful parse
    cache->cached_path = current_path;
    cache->is_cached = true;
    
    updateFileListUI();
}

void UITabFiles::updateFileListUI() {
    if (!file_list_container) return;
    
    // Get current storage cache
    StorageCache* cache = getCurrentCache();
    
    // Clear existing file buttons
    lv_obj_clean(file_list_container);
    
    if (cache->file_list.empty()) {
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
    size_t max_files = std::min(cache->file_list.size(), (size_t)100);
    
    // Create file/directory entries
    for (size_t i = 0; i < max_files; i++) {
        const FileInfo &file = cache->file_list[i];
        
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
            snprintf(label_text, sizeof(label_text), LV_SYMBOL_DIRECTORY " %s", file.name.c_str());
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
            
            // Show upload button for Display SD, or play/delete for FluidNC storage
            if (current_storage == StorageSource::DISPLAY_SD) {
                // Upload button (for Display SD files)
                lv_obj_t *btn_upload = lv_button_create(file_row);
                lv_obj_set_size(btn_upload, 120, 38);
                lv_obj_align(btn_upload, LV_ALIGN_RIGHT_MID, -5, 0);
                lv_obj_set_style_bg_color(btn_upload, UITheme::ACCENT_PRIMARY, 0);
                lv_obj_set_style_radius(btn_upload, 3, 0);
                lv_obj_add_event_cb(btn_upload, upload_button_event_cb, LV_EVENT_CLICKED, filenames_storage[i]);
                
                lv_obj_t *lbl_upload = lv_label_create(btn_upload);
                lv_label_set_text(lbl_upload, LV_SYMBOL_UPLOAD " Upload");
                lv_obj_set_style_text_font(lbl_upload, &lv_font_montserrat_18, 0);
                lv_obj_center(lbl_upload);
            } else {
                // Delete button (for FluidNC files)
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
                
                // Play button (for FluidNC files)
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
            }
        }  // End of if (!file.is_directory)
    }  // End of for loop
    
    if (status_label) {
        char buf[64];
        int file_count = 0;
        int dir_count = 0;
        for (const auto &f : cache->file_list) {
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

// List files from Display SD card
void UITabFiles::listDisplaySDFiles(const std::string &path) {
    Serial.printf("[Files] Listing Display SD: %s\n", path.c_str());
    
    // Check if SD card is available
    if (!isDisplaySDAvailable()) {
        Serial.println("[Files] SD card not available");
        display_sd_cache.is_cached = false;
        display_sd_cache.file_list.clear();
        if (status_label) {
            lv_label_set_text(status_label, "SD card not available");
            lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        }
        if (path_label) {
            lv_label_set_text(path_label, "/");
        }
        // Clear file list UI
        if (file_list_container) {
            lv_obj_clean(file_list_container);
        }
        return;
    }
    
    // Update current path
    current_path = path;
    
    // Get Display SD cache
    display_sd_cache.file_list.clear();
    
    File root = SD.open(path.c_str());
    if (!root || !root.isDirectory()) {
        Serial.println("[Files] Failed to open Display SD directory");
        lv_label_set_text(status_label, "Failed to open directory");
        lv_obj_set_style_text_color(status_label, UITheme::UI_WARNING, 0);
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        FileInfo info;
        info.name = file.name();
        
        // Remove leading path from name
        size_t lastSlash = info.name.find_last_of('/');
        if (lastSlash != std::string::npos) {
            info.name = info.name.substr(lastSlash + 1);
        }
        
        // Skip "System Volume Information" folder
        if (info.name == "System Volume Information") {
            file = root.openNextFile();
            continue;
        }
        
        info.is_directory = file.isDirectory();
        info.size = info.is_directory ? -1 : file.size();
        
        display_sd_cache.file_list.push_back(info);
        file = root.openNextFile();
    }
    
    root.close();
    
    // Sort files (folders on top if preference is set)
    Preferences prefs;
    prefs.begin(PREFS_SYSTEM_NAMESPACE, true);
    bool folders_on_top = prefs.getBool("folders_on_top", false);
    prefs.end();
    
    if (folders_on_top) {
        std::sort(display_sd_cache.file_list.begin(), display_sd_cache.file_list.end(),
                 [](const FileInfo &a, const FileInfo &b) {
                     if (a.is_directory != b.is_directory) {
                         return a.is_directory;  // Folders first
                     }
                     return a.name < b.name;
                 });
    } else {
        std::sort(display_sd_cache.file_list.begin(), display_sd_cache.file_list.end(),
                 [](const FileInfo &a, const FileInfo &b) {
                     return a.name < b.name;
                 });
    }
    
    Serial.printf("[Files] Found %d items on Display SD\n", display_sd_cache.file_list.size());
    
    // Mark cache as valid after successful list
    display_sd_cache.cached_path = current_path;
    display_sd_cache.is_cached = true;
    
    // Update path label
    if (path_label) {
        lv_label_set_text(path_label, current_path.c_str());
    }
    
    // Update UI
    updateFileListUI();
}

// Upload button event callback
void UITabFiles::upload_button_event_cb(lv_event_t *e) {
    const char* fullPath = (const char*)lv_event_get_user_data(e);
    
    // Check if SD card is still available
    if (!isDisplaySDAvailable()) {
        Serial.println("[Files] SD card not available for upload");
        // Create error dialog
        lv_obj_t *dialog = lv_obj_create(lv_scr_act());
        lv_obj_set_size(dialog, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(dialog, lv_color_make(0, 0, 0), 0);
        lv_obj_set_style_bg_opa(dialog, LV_OPA_70, 0);
        lv_obj_set_style_border_width(dialog, 0, 0);
        lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t *content = lv_obj_create(dialog);
        lv_obj_set_size(content, 500, 200);
        lv_obj_center(content);
        lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
        lv_obj_set_style_border_color(content, UITheme::UI_WARNING, 0);
        lv_obj_set_style_border_width(content, 3, 0);
        
        lv_obj_t *label = lv_label_create(content);
        lv_label_set_text(label, "SD card not available.\nPlease insert SD card.");
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, -20);
        
        lv_obj_t *btn = lv_btn_create(content);
        lv_obj_set_size(btn, 120, 45);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_add_event_cb(btn, [](lv_event_t *e) {
            lv_obj_delete((lv_obj_t*)lv_event_get_user_data(e));
        }, LV_EVENT_CLICKED, dialog);
        
        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, "OK");
        lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
        lv_obj_center(btn_label);
        
        return;
    }
    
    // Extract just the filename from the full path for display
    std::string pathStr(fullPath);
    size_t lastSlash = pathStr.find_last_of('/');
    const char* filename = (lastSlash != std::string::npos) ? fullPath + lastSlash + 1 : fullPath;
    
    // Get file size
    File file = SD.open(fullPath);
    if (!file) {
        Serial.printf("[Files] Failed to open file: %s\n", fullPath);
        return;
    }
    
    size_t fileSize = file.size();
    file.close();
    
    Serial.printf("[Files] File size: %zu bytes (%.2f MB)\n", fileSize, fileSize / (1024.0f * 1024.0f));
    
    showUploadDialog(filename, fullPath, fileSize);
}

// Show upload confirmation dialog
void UITabFiles::showUploadDialog(const char* filename, const char* fullPath, size_t fileSize) {
    if (upload_dialog) {
        lv_obj_delete(upload_dialog);
    }
    
    // Create modal background
    upload_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(upload_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(upload_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(upload_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(upload_dialog, 0, 0);
    lv_obj_clear_flag(upload_dialog, LV_OBJ_FLAG_SCROLLABLE);
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(upload_dialog);
    lv_obj_set_size(content, 600, 300);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_UPLOAD " Upload to FluidNC");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    
    // Filename
    lv_obj_t *lbl_filename = lv_label_create(content);
    lv_label_set_text_fmt(lbl_filename, "File: %s", filename);
    lv_obj_set_style_text_font(lbl_filename, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_filename, UITheme::TEXT_LIGHT, 0);
    lv_label_set_long_mode(lbl_filename, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_filename, 550);
    lv_obj_align(lbl_filename, LV_ALIGN_TOP_LEFT, 0, 45);
    
    // File size
    lv_obj_t *lbl_size = lv_label_create(content);
    float sizeMB = fileSize / (1024.0f * 1024.0f);
    char sizeText[64];
    if (sizeMB >= 1.0f) {
        snprintf(sizeText, sizeof(sizeText), "Size: %.2f MB", sizeMB);
    } else {
        float sizeKB = fileSize / 1024.0f;
        snprintf(sizeText, sizeof(sizeText), "Size: %.2f KB", sizeKB);
    }
    lv_label_set_text(lbl_size, sizeText);
    lv_obj_set_style_text_font(lbl_size, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_size, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(lbl_size, LV_ALIGN_TOP_LEFT, 0, 80);
    
    // Destination
    lv_obj_t *lbl_dest = lv_label_create(content);
    lv_label_set_text_fmt(lbl_dest, "Destination: /sd%s%s", FLUIDNC_UPLOAD_PATH, filename);
    lv_obj_set_style_text_font(lbl_dest, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_dest, UITheme::TEXT_LIGHT, 0);
    lv_label_set_long_mode(lbl_dest, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_dest, 550);
    lv_obj_align(lbl_dest, LV_ALIGN_TOP_LEFT, 0, 115);
    
    // Button container (positioned at bottom)
    lv_obj_t *btn_container = lv_obj_create(content);
    lv_obj_set_size(btn_container, 560, 60);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 0, 0);
    lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Upload button (left side)
    lv_obj_t *btn_upload = lv_btn_create(btn_container);
    lv_obj_set_size(btn_upload, 180, 50);
    lv_obj_set_style_bg_color(btn_upload, UITheme::ACCENT_PRIMARY, 0);
    
    // Store filename and full path for upload callback
    static char upload_filename[128];
    static char upload_fullpath[256];
    strncpy(upload_filename, filename, sizeof(upload_filename) - 1);
    upload_filename[sizeof(upload_filename) - 1] = '\0';
    strncpy(upload_fullpath, fullPath, sizeof(upload_fullpath) - 1);
    upload_fullpath[sizeof(upload_fullpath) - 1] = '\0';
    
    lv_obj_add_event_cb(btn_upload, [](lv_event_t *e) {
        const char* fname = (const char*)lv_event_get_user_data(e);
        
        Serial.printf("[UITabFiles] Upload button clicked for: %s\n", fname);
        
        // Copy filename and full path to heap for timer
        char* fname_copy = (char*)malloc(128);
        char* fullpath_copy = (char*)malloc(256);
        strncpy(fname_copy, upload_filename, 127);
        fname_copy[127] = '\0';
        strncpy(fullpath_copy, upload_fullpath, 255);
        fullpath_copy[255] = '\0';
        
        // Close confirmation dialog first
        if (upload_dialog) {
            lv_obj_delete(upload_dialog);
            upload_dialog = nullptr;
        }
        
        // Give LVGL time to process the deletion before creating new dialog
        lv_timer_create([](lv_timer_t *timer) {
            char* fullpath_copy = (char*)lv_timer_get_user_data(timer);
            
            // Extract filename from stored copies
            size_t lastSlash = std::string(fullpath_copy).find_last_of('/');
            const char* fname = (lastSlash != std::string::npos) ? fullpath_copy + lastSlash + 1 : fullpath_copy;
            
            Serial.printf("[UITabFiles] Timer callback executing for: %s\n", fname);
            
            // Show progress dialog
            showUploadProgress(fname);
            
            // Use the stored full path directly
            UploadManager::uploadFile(
                fullpath_copy,
                fname,
                updateUploadProgress,
                closeUploadProgress
            );
            
            free(fullpath_copy);
            lv_timer_delete(timer);
        }, 50, fullpath_copy);
        
        free(fname_copy);
    }, LV_EVENT_CLICKED, upload_filename);
    
    lv_obj_t *lbl_upload = lv_label_create(btn_upload);
    lv_label_set_text(lbl_upload, "Upload");
    lv_obj_set_style_text_font(lbl_upload, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_upload);
    
    // Cancel button (right side)
    lv_obj_t *btn_cancel = lv_btn_create(btn_container);
    lv_obj_set_size(btn_cancel, 180, 50);
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_BUTTON, 0);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t *e) {
        lv_obj_delete(upload_dialog);
        upload_dialog = nullptr;
    }, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_cancel);
}

// Show upload progress dialog
void UITabFiles::showUploadProgress(const char* filename) {
    Serial.printf("[UITabFiles] showUploadProgress called for: %s\n", filename);
    
    if (upload_progress_dialog) {
        Serial.println("[UITabFiles] Deleting existing progress dialog");
        lv_obj_delete(upload_progress_dialog);
        upload_progress_dialog = nullptr;
        upload_progress_bar = nullptr;
        upload_progress_label = nullptr;
    }
    
    // Create modal background
    upload_progress_dialog = lv_obj_create(lv_scr_act());
    if (!upload_progress_dialog) {
        Serial.println("[UITabFiles] ERROR: Failed to create upload_progress_dialog!");
        return;
    }
    
    lv_obj_set_size(upload_progress_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(upload_progress_dialog, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(upload_progress_dialog, LV_OPA_70, 0);
    lv_obj_set_style_border_width(upload_progress_dialog, 0, 0);
    lv_obj_clear_flag(upload_progress_dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(upload_progress_dialog);  // Ensure it's on top
    
    // Dialog content box
    lv_obj_t *content = lv_obj_create(upload_progress_dialog);
    lv_obj_set_size(content, 600, 280);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_color(content, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_border_width(content, 3, 0);
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    Serial.printf("[UITabFiles] Upload progress dialog created at %p\n", upload_progress_dialog);
    
    // Title
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_UPLOAD " Uploading...");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(title, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    
    // Filename
    lv_obj_t *lbl_filename = lv_label_create(content);
    lv_label_set_text_fmt(lbl_filename, "File: %s", filename);
    lv_obj_set_style_text_font(lbl_filename, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_filename, UITheme::TEXT_LIGHT, 0);
    lv_label_set_long_mode(lbl_filename, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lbl_filename, 550);
    lv_obj_align(lbl_filename, LV_ALIGN_TOP_LEFT, 0, 45);
    
    // Progress bar
    upload_progress_bar = lv_bar_create(content);
    lv_obj_set_size(upload_progress_bar, 560, 30);
    lv_obj_align(upload_progress_bar, LV_ALIGN_TOP_LEFT, 0, 85);
    lv_bar_set_value(upload_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(upload_progress_bar, UITheme::BG_DARKER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(upload_progress_bar, UITheme::ACCENT_PRIMARY, LV_PART_INDICATOR);
    
    Serial.printf("[UITabFiles] Progress bar created at %p\n", upload_progress_bar);
    
    // Progress label
    upload_progress_label = lv_label_create(content);
    lv_label_set_text(upload_progress_label, "0 KB / 0 KB (0%)");
    lv_obj_set_style_text_font(upload_progress_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(upload_progress_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(upload_progress_label, LV_ALIGN_TOP_LEFT, 0, 130);
    
    Serial.printf("[UITabFiles] Progress label created at %p\n", upload_progress_label);
    
    // Info text at bottom (will be replaced with Close button on success)
    lv_obj_t *lbl_info = lv_label_create(content);
    lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_info, UITheme::TEXT_DISABLED, 0);
    lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(lbl_info, "Reset device to cancel upload");
    
    // Force immediate screen refresh to show dialog before upload blocks
    Serial.println("[UITabFiles] Forcing screen refresh...");
    lv_refr_now(NULL);
    Serial.println("[UITabFiles] Screen refresh complete");
}

// Update upload progress callback
void UITabFiles::updateUploadProgress(size_t current, size_t total) {
    Serial.printf("[UITabFiles] updateUploadProgress called: %d / %d\n", current, total);
    
    if (!upload_progress_bar || !upload_progress_label) {
        Serial.println("[UITabFiles] WARNING: upload_progress_bar or upload_progress_label is null!");
        return;
    }
    
    int percent = (current * 100) / total;
    lv_bar_set_value(upload_progress_bar, percent, LV_ANIM_OFF);
    
    if (total >= 1048576) {
        // Show in MB
        int currentMB = current / 10485;  // current / 1024 / 1024 * 100 = current / 10485.76
        int totalMB = total / 10485;
        lv_label_set_text_fmt(upload_progress_label, "%d.%02d MB / %d.%02d MB (%d%%)",
                             currentMB / 100, currentMB % 100, totalMB / 100, totalMB % 100, percent);
    } else if (total >= 1024) {
        // Show in KB
        int currentKB = current / 1024;
        int totalKB = total / 1024;
        lv_label_set_text_fmt(upload_progress_label, "%d KB / %d KB (%d%%)",
                             currentKB, totalKB, percent);
    } else {
        // Show in bytes for files smaller than 1 KB
        lv_label_set_text_fmt(upload_progress_label, "%d bytes / %d bytes (%d%%)",
                             current, total, percent);
    }
    
    // Force screen refresh to show updated progress
    lv_refr_now(NULL);
}

// Close upload progress dialog
void UITabFiles::closeUploadProgress(bool success, const char* error) {
    Serial.printf("[UITabFiles] closeUploadProgress called: success=%d, error=%s\n", success, error ? error : "null");
    
    if (!upload_progress_dialog) {
        Serial.println("[UITabFiles] upload_progress_dialog is null, already closed");
        return;
    }
    
    if (success) {
        // Update title and show success
        lv_obj_t *content = lv_obj_get_child(upload_progress_dialog, 0);
        if (content) {
            lv_obj_t *title = lv_obj_get_child(content, 0);
            if (title) {
                lv_label_set_text(title, LV_SYMBOL_OK " UPLOAD COMPLETE");
                lv_obj_set_style_text_color(title, UITheme::UI_SUCCESS, 0);
            }
            
            // Remove the info text (it's the last child before we add the button)
            uint32_t child_count = lv_obj_get_child_count(content);
            if (child_count > 0) {
                lv_obj_t *last_child = lv_obj_get_child(content, child_count - 1);
                if (last_child) {
                    lv_obj_delete(last_child);
                }
            }
            
            // Add Close button at bottom
            lv_obj_t *btn_close = lv_btn_create(content);
            lv_obj_set_size(btn_close, 180, 50);
            lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_color(btn_close, UITheme::ACCENT_PRIMARY, 0);
            lv_obj_add_event_cb(btn_close, [](lv_event_t *e) {
                if (upload_progress_dialog) {
                    lv_obj_delete(upload_progress_dialog);
                    upload_progress_dialog = nullptr;
                    upload_progress_bar = nullptr;
                    upload_progress_label = nullptr;
                }
            }, LV_EVENT_CLICKED, nullptr);
            
            lv_obj_t *lbl_close = lv_label_create(btn_close);
            lv_label_set_text(lbl_close, "Close");
            lv_obj_set_style_text_font(lbl_close, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_close);
        }
        
        // Force screen refresh to show the title change and button
        lv_refr_now(NULL);
    } else {
        // Show error or cancellation message
        lv_obj_t *content = lv_obj_get_child(upload_progress_dialog, 0);
        if (content) {
            lv_obj_t *title = lv_obj_get_child(content, 0);
            if (title) {
                if (error && strstr(error, "cancelled")) {
                    lv_label_set_text(title, LV_SYMBOL_CLOSE " UPLOAD CANCELLED");
                    lv_obj_set_style_text_color(title, UITheme::UI_WARNING, 0);
                } else {
                    lv_label_set_text_fmt(title, LV_SYMBOL_WARNING " UPLOAD FAILED");
                    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
                }
            }
            
            // Show error message if provided
            if (error && !strstr(error, "cancelled")) {
                lv_obj_t *error_label = lv_label_create(content);
                lv_label_set_text(error_label, error);
                lv_obj_set_style_text_font(error_label, &lv_font_montserrat_16, 0);
                lv_obj_set_style_text_color(error_label, UITheme::TEXT_LIGHT, 0);
                lv_obj_align(error_label, LV_ALIGN_TOP_LEFT, 0, 180);
            }
            
            // Change cancel button to close button
            // Find the cancel button (it's the last child of content)
            uint32_t child_count = lv_obj_get_child_count(content);
            if (child_count > 0) {
                lv_obj_t *cancel_btn = lv_obj_get_child(content, child_count - 1);
                if (cancel_btn) {
                    // Clear old event callbacks
                    lv_obj_remove_event_cb(cancel_btn, nullptr);
                    lv_obj_set_style_bg_color(cancel_btn, UITheme::BG_BUTTON, 0);
                    lv_obj_add_event_cb(cancel_btn, [](lv_event_t *e) {
                        Serial.println("[UITabFiles] Close button clicked");
                        if (upload_progress_dialog) {
                            lv_obj_delete(upload_progress_dialog);
                            upload_progress_dialog = nullptr;
                            upload_progress_bar = nullptr;
                            upload_progress_label = nullptr;
                        }
                    }, LV_EVENT_CLICKED, nullptr);
                    
                    // Update button label
                    lv_obj_t *btn_label = lv_obj_get_child(cancel_btn, 0);
                    if (btn_label) {
                        lv_label_set_text(btn_label, "Close");
                    }
                }
            }
        }
    }
}
