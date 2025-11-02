#include "ui/tabs/settings/ui_tab_settings_about.h"
#include "ui/ui_theme.h"
#include "config.h"
#include "network/screenshot_server.h"
#include <WiFi.h>

// Static member initialization
lv_obj_t *UITabSettingsAbout::screenshot_link_label = nullptr;
lv_obj_t *UITabSettingsAbout::screenshot_qr = nullptr;
bool UITabSettingsAbout::wifi_url_set = false;

void UITabSettingsAbout::create(lv_obj_t *tab) {
    // Disable scrolling on the tab itself
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create container for content
    lv_obj_t *container = lv_obj_create(tab);
    lv_obj_set_size(container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(container, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 20, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(container, 15, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Project name (large, colored)
    lv_obj_t *project_name = lv_label_create(container);
    lv_label_set_text(project_name, "FluidTouch");
    lv_obj_set_style_text_font(project_name, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(project_name, UITheme::ACCENT_PRIMARY, 0);
    
    // Version (medium, gray)
    lv_obj_t *version = lv_label_create(container);
    lv_label_set_text(version, "Version: " FLUIDTOUCH_VERSION);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(version, UITheme::TEXT_MEDIUM, 0);
    
    // Horizontal container for both columns
    lv_obj_t *columns_container = lv_obj_create(container);
    lv_obj_set_size(columns_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(columns_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(columns_container, 0, 0);
    lv_obj_set_style_pad_all(columns_container, 0, 0);
    lv_obj_set_flex_flow(columns_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(columns_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(columns_container, 60, 0);
    lv_obj_set_style_pad_top(columns_container, 20, 0);
    
    // Left column: GitHub
    lv_obj_t *github_column = lv_obj_create(columns_container);
    lv_obj_set_size(github_column, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(github_column, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(github_column, 0, 0);
    lv_obj_set_style_pad_all(github_column, 0, 0);
    lv_obj_set_flex_flow(github_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(github_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(github_column, 10, 0);
    
    // GitHub title
    lv_obj_t *github_title = lv_label_create(github_column);
    lv_label_set_text(github_title, "Documentation & Source Code");
    lv_obj_set_style_text_font(github_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(github_title, UITheme::ACCENT_SECONDARY, 0);
    
    // GitHub link
    lv_obj_t *github_link = lv_label_create(github_column);
    lv_label_set_text(github_link, "https://github.com/jeyeager65/FluidTouch");
    lv_obj_set_style_text_font(github_link, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(github_link, UITheme::UI_INFO, 0);
    lv_obj_set_style_text_align(github_link, LV_TEXT_ALIGN_CENTER, 0);
    
    // GitHub QR code
    lv_obj_t *github_qr = lv_qrcode_create(github_column);
    lv_qrcode_set_size(github_qr, 150);
    lv_qrcode_set_dark_color(github_qr, lv_color_white());
    lv_qrcode_set_light_color(github_qr, UITheme::BG_MEDIUM);
    lv_qrcode_update(github_qr, "https://github.com/jeyeager65/FluidTouch", strlen("https://github.com/jeyeager65/FluidTouch"));
    
    // Right column: Screenshot Server
    lv_obj_t *screenshot_column = lv_obj_create(columns_container);
    lv_obj_set_size(screenshot_column, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(screenshot_column, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(screenshot_column, 0, 0);
    lv_obj_set_style_pad_all(screenshot_column, 0, 0);
    lv_obj_set_flex_flow(screenshot_column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screenshot_column, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(screenshot_column, 10, 0);
    
    // Screenshot title
    lv_obj_t *screenshot_title = lv_label_create(screenshot_column);
    lv_label_set_text(screenshot_title, "Screenshot Server");
    lv_obj_set_style_text_font(screenshot_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(screenshot_title, UITheme::ACCENT_SECONDARY, 0);
    
    // Screenshot link
    screenshot_link_label = lv_label_create(screenshot_column);
    lv_obj_set_style_text_font(screenshot_link_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(screenshot_link_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // Screenshot QR code
    screenshot_qr = lv_qrcode_create(screenshot_column);
    lv_qrcode_set_size(screenshot_qr, 150);
    lv_qrcode_set_dark_color(screenshot_qr, lv_color_white());
    lv_qrcode_set_light_color(screenshot_qr, UITheme::BG_MEDIUM);
    
    update();  // Set initial text and QR code
}

void UITabSettingsAbout::update() {
    if (screenshot_link_label == nullptr || screenshot_qr == nullptr) return;
    
    // Only update once when WiFi connects
    if (wifi_url_set) return;
    
    if (WiFi.status() == WL_CONNECTED) {
        String url = "http://" + WiFi.localIP().toString();
        lv_label_set_text(screenshot_link_label, url.c_str());
        lv_obj_set_style_text_color(screenshot_link_label, UITheme::UI_INFO, 0);
        
        // Update QR code with URL
        lv_qrcode_update(screenshot_qr, url.c_str(), url.length());
        lv_obj_clear_flag(screenshot_qr, LV_OBJ_FLAG_HIDDEN);
        
        wifi_url_set = true;  // Mark as set, don't update again
    } else {
        lv_label_set_text(screenshot_link_label, "WiFi not\nconnected");
        lv_obj_set_style_text_color(screenshot_link_label, UITheme::TEXT_DISABLED, 0);
        
        // Hide QR code when not connected
        lv_obj_add_flag(screenshot_qr, LV_OBJ_FLAG_HIDDEN);
    }
}
