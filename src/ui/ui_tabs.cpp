#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "ui/tabs/ui_tab_status.h"
#include "ui/tabs/ui_tab_control.h"
#include "ui/tabs/ui_tab_files.h"
#include "ui/tabs/ui_tab_macros.h"
#include "ui/tabs/ui_tab_terminal.h"
#include "ui/tabs/ui_tab_settings.h"
#include "network/fluidnc_client.h"

// Static member initialization
lv_obj_t *UITabs::tabview = nullptr;
lv_obj_t *UITabs::tab_status = nullptr;
lv_obj_t *UITabs::tab_control = nullptr;
lv_obj_t *UITabs::tab_files = nullptr;
lv_obj_t *UITabs::tab_macros = nullptr;
lv_obj_t *UITabs::tab_terminal = nullptr;
lv_obj_t *UITabs::tab_settings = nullptr;

// Create main tabview and all tabs
void UITabs::createTabs() {
    // Create tabview
    tabview = lv_tabview_create(lv_screen_active());
    lv_obj_set_size(tabview, SCREEN_WIDTH, SCREEN_HEIGHT - STATUS_BAR_HEIGHT);
    lv_obj_align(tabview, LV_ALIGN_TOP_LEFT, 0, 0);  // Align to top-left, not bottom
    
    // Get the tab bar and set height
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tabview);
    lv_obj_set_height(tab_bar, TAB_BUTTON_HEIGHT);
    
    // Remove padding from tabview content area
    lv_obj_set_style_pad_all(lv_tabview_get_content(tabview), 0, 0);
    
    // Disable scrolling on tab buttons and content
    lv_obj_clear_flag(tab_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(lv_tabview_get_content(tabview), LV_OBJ_FLAG_SCROLLABLE);
    
    // Style tab buttons with larger font
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_20, 0);  // Direct to tab bar
    lv_obj_set_style_text_font(tabview, &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tabview, UITheme::BG_BUTTON, LV_PART_ITEMS);
    lv_obj_set_style_text_color(tabview, UITheme::TEXT_LIGHT, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(tabview, UITheme::ACCENT_PRIMARY, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(tabview, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);
    
    // Add tabs
    tab_status = lv_tabview_add_tab(tabview, "Status");
    tab_control = lv_tabview_add_tab(tabview, "Control");
    tab_files = lv_tabview_add_tab(tabview, "Files");
    tab_macros = lv_tabview_add_tab(tabview, "Macros");
    tab_terminal = lv_tabview_add_tab(tabview, "Terminal");
    tab_settings = lv_tabview_add_tab(tabview, "Settings");
    
    // Disable scrolling only on tabs that don't need it
    lv_obj_clear_flag(tab_status, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_control, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_macros, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_files, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_terminal, LV_OBJ_FLAG_SCROLLABLE);
    // Settings tab may need scrolling, so leave it enabled
    
    // Create tab content
    createStatusTab(tab_status);
    createControlTab(tab_control);
    createFilesTab(tab_files);
    createMacrosTab(tab_macros);
    createTerminalTab(tab_terminal);
    createSettingsTab(tab_settings);
    
    // Add event handler for tab changes
    lv_obj_add_event_cb(tabview, tab_changed_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);
}

// Tab change event handler
void UITabs::tab_changed_event_cb(lv_event_t *e) {
    lv_obj_t *tabview = (lv_obj_t*)lv_event_get_target(e);
    uint32_t active_tab = lv_tabview_get_tab_active(tabview);
    
    // Tab indices: 0=Status, 1=Control, 2=Files, 3=Macros, 4=Terminal, 5=Settings
    if (active_tab == 2) {  // Files tab
        // Trigger initial load on first selection
        UITabFiles::refreshFileList();
    }
}

// Create Status tab content (delegated to UITabStatus module)
void UITabs::createStatusTab(lv_obj_t *tab) {
    UITabStatus::create(tab);
}

// Create Control tab content (delegated to UITabControl module)
void UITabs::createControlTab(lv_obj_t *tab) {
    UITabControl::create(tab);
}

// Create Files tab content (delegated to UITabFiles module)
void UITabs::createFilesTab(lv_obj_t *tab) {
    UITabFiles::create(tab);
}

// Create Macros tab content (delegated to UITabMacros module)
void UITabs::createMacrosTab(lv_obj_t *tab) {
    UITabMacros::create(tab);
}

// Create Terminal tab content (delegated to UITabTerminal module)
void UITabs::createTerminalTab(lv_obj_t *tab) {
    UITabTerminal::create(tab);

    // Register terminal callback to receive FluidNC messages (excluding status reports)
    FluidNCClient::setTerminalCallback([](const char* message) {
        UITabTerminal::appendMessage(message);
    });
}

// Create Settings tab content (delegated to UITabSettings module)
void UITabs::createSettingsTab(lv_obj_t *tab) {
    UITabSettings::create(tab);
}

// Public wrappers for settings functions
void UITabs::loadSettings() {
    // Settings are now loaded by the individual subtabs (General and Connection)
}

void UITabs::saveSettings() {
    // Settings are now saved by the individual subtabs (General and Connection)
}
