#include "ui/tabs/ui_tab_test.h"
#include <Arduino.h>
#include <WiFi.h>

// Helper to create a metric card
static lv_obj_t* create_metric_card(lv_obj_t* parent, const char* title, const char* value, lv_color_t color) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 175, 100);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_color(card, color, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title
    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xAAAAAA), 0);

    // Value
    lv_obj_t *value_label = lv_label_create(card);
    lv_label_set_text(value_label, value);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(value_label, color, 0);

    return card;
}

// Helper to create a status indicator
static lv_obj_t* create_status_indicator(lv_obj_t* parent, const char* label_text, bool is_active) {
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 230, 50);
    lv_obj_set_style_bg_color(container, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_radius(container, 6, 0);
    lv_obj_set_style_pad_all(container, 8, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // LED indicator
    lv_obj_t *led = lv_led_create(container);
    lv_obj_set_size(led, 20, 20);
    if (is_active) {
        lv_led_set_color(led, lv_color_hex(0x4CAF50));
        lv_led_on(led);
    } else {
        lv_led_set_color(led, lv_color_hex(0x666666));
        lv_led_off(led);
    }

    // Label
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_pad_left(label, 10, 0);

    return container;
}

void UITabTest::create(lv_obj_t *tab) {
    // Main container with scrolling
    lv_obj_t *content = lv_obj_create(tab);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(content, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 15, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // ===== HEADER =====
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " CNC Machine Monitor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00AA88), 0);
    lv_obj_set_style_pad_bottom(title, 15, 0);

    // ===== SYSTEM METRICS ROW =====
    lv_obj_t *metrics_row = lv_obj_create(content);
    lv_obj_set_size(metrics_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(metrics_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(metrics_row, 0, 0);
    lv_obj_set_style_pad_all(metrics_row, 0, 0);
    lv_obj_set_flex_flow(metrics_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(metrics_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Calculate system metrics
    char temp_str[32];
    
    // CPU Temperature (if available, otherwise uptime)
    uint32_t uptime_sec = millis() / 1000;
    snprintf(temp_str, sizeof(temp_str), "%02lu:%02lu:%02lu", 
             uptime_sec / 3600, (uptime_sec % 3600) / 60, uptime_sec % 60);
    create_metric_card(metrics_row, "Uptime", temp_str, lv_color_hex(0x2196F3));

    // Free PSRAM
    snprintf(temp_str, sizeof(temp_str), "%.1f MB", ESP.getFreePsram() / 1024.0 / 1024.0);
    create_metric_card(metrics_row, "Free PSRAM", temp_str, lv_color_hex(0x9C27B0));

    // Free Heap
    snprintf(temp_str, sizeof(temp_str), "%d KB", ESP.getFreeHeap() / 1024);
    create_metric_card(metrics_row, "Free Heap", temp_str, lv_color_hex(0xFF9800));

    // WiFi Signal
    snprintf(temp_str, sizeof(temp_str), "%d dBm", WiFi.RSSI());
    create_metric_card(metrics_row, "WiFi RSSI", temp_str, lv_color_hex(0x4CAF50));

    // ===== CONNECTIVITY STATUS =====
    lv_obj_t *conn_title = lv_label_create(content);
    lv_label_set_text(conn_title, "Connectivity");
    lv_obj_set_style_text_font(conn_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(conn_title, lv_color_hex(0x00BCD4), 0);
    lv_obj_set_style_pad_top(conn_title, 15, 0);
    lv_obj_set_style_pad_bottom(conn_title, 10, 0);

    lv_obj_t *conn_row = lv_obj_create(content);
    lv_obj_set_size(conn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(conn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(conn_row, 0, 0);
    lv_obj_set_style_pad_all(conn_row, 0, 0);
    lv_obj_set_flex_flow(conn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(conn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    bool wifi_connected = WiFi.status() == WL_CONNECTED;
    create_status_indicator(conn_row, "WiFi Connected", wifi_connected);
    create_status_indicator(conn_row, "FluidNC Link", false); // TODO: Check actual FluidNC connection
    create_status_indicator(conn_row, "SD Card Ready", false); // TODO: Check SD card

    // ===== MACHINE AXES STATUS =====
    lv_obj_t *axes_title = lv_label_create(content);
    lv_label_set_text(axes_title, "Axis Indicators");
    lv_obj_set_style_text_font(axes_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(axes_title, lv_color_hex(0x00BCD4), 0);
    lv_obj_set_style_pad_top(axes_title, 15, 0);
    lv_obj_set_style_pad_bottom(axes_title, 10, 0);

    lv_obj_t *axes_row = lv_obj_create(content);
    lv_obj_set_size(axes_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(axes_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(axes_row, 0, 0);
    lv_obj_set_style_pad_all(axes_row, 0, 0);
    lv_obj_set_flex_flow(axes_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(axes_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // TODO: Get actual axis states from FluidNC
    create_status_indicator(axes_row, "X-Axis Homed", false);
    create_status_indicator(axes_row, "Y-Axis Homed", false);
    create_status_indicator(axes_row, "Z-Axis Homed", false);

    // ===== SAFETY STATUS =====
    lv_obj_t *safety_title = lv_label_create(content);
    lv_label_set_text(safety_title, "Safety & Limits");
    lv_obj_set_style_text_font(safety_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(safety_title, lv_color_hex(0x00BCD4), 0);
    lv_obj_set_style_pad_top(safety_title, 15, 0);
    lv_obj_set_style_pad_bottom(safety_title, 10, 0);

    lv_obj_t *safety_row = lv_obj_create(content);
    lv_obj_set_size(safety_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(safety_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(safety_row, 0, 0);
    lv_obj_set_style_pad_all(safety_row, 0, 0);
    lv_obj_set_flex_flow(safety_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(safety_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // TODO: Get actual limit switch states from FluidNC
    create_status_indicator(safety_row, "Limits OK", true);
    create_status_indicator(safety_row, "E-Stop Clear", true);
    create_status_indicator(safety_row, "Probe Detected", false);

    // ===== INFO NOTE =====
    lv_obj_t *note = lv_label_create(content);
    lv_label_set_text(note, 
        LV_SYMBOL_WARNING " Diagnostic Mode\n"
        "This monitor displays system health and machine status.\n"
        "Real-time FluidNC integration coming soon!");
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note, lv_pct(95));
    lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(note, lv_color_hex(0xFFB74D), 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(note, 20, 0);
}
