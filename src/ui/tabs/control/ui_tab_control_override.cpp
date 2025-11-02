#include "ui/tabs/control/ui_tab_control_override.h"
#include "ui/ui_theme.h"
#include "config.h"
#include "network/fluidnc_client.h"

// Static label pointers
lv_obj_t* UITabControlOverride::lbl_feed_value = nullptr;
lv_obj_t* UITabControlOverride::lbl_rapid_value = nullptr;
lv_obj_t* UITabControlOverride::lbl_spindle_value = nullptr;

// Feed override event handlers
static void feed_coarse_plus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x91, 0};  // FeedOvrCoarsePlus (+10%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Feed Override: +10%");
    }
}

static void feed_coarse_minus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x92, 0};  // FeedOvrCoarseMinus (-10%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Feed Override: -10%");
    }
}

static void feed_fine_plus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x93, 0};  // FeedOvrFinePlus (+1%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Feed Override: +1%");
    }
}

static void feed_fine_minus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x94, 0};  // FeedOvrFineMinus (-1%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Feed Override: -1%");
    }
}

static void feed_reset_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x90, 0};  // FeedOvrReset (100%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Feed Override: Reset to 100%");
    }
}

// Rapid override event handlers
static void rapid_100_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x95, 0};  // RapidOvrReset (100%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Rapid Override: 100%");
    }
}

static void rapid_50_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x96, 0};  // RapidOvrMedium (50%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Rapid Override: 50%");
    }
}

static void rapid_25_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x97, 0};  // RapidOvrLow (25%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Rapid Override: 25%");
    }
}

// Spindle override event handlers
static void spindle_coarse_plus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x9A, 0};  // SpindleOvrCoarsePlus (+10%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Spindle Override: +10%");
    }
}

static void spindle_coarse_minus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x9B, 0};  // SpindleOvrCoarseMinus (-10%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Spindle Override: -10%");
    }
}

static void spindle_fine_plus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x9C, 0};  // SpindleOvrFinePlus (+1%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Spindle Override: +1%");
    }
}

static void spindle_fine_minus_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x9D, 0};  // SpindleOvrFineMinus (-1%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Spindle Override: -1%");
    }
}

static void spindle_reset_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        char cmd[2] = {0x99, 0};  // SpindleOvrReset (100%)
        FluidNCClient::sendCommand(cmd);
        Serial.println("Spindle Override: Reset to 100%");
    }
}

lv_obj_t* UITabControlOverride::create(lv_obj_t* parent) {
    // Create main container with 3 columns
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 5, 0);
    lv_obj_set_style_pad_column(cont, 8, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    
    // === COLUMN 1: FEED OVERRIDE ===
    lv_obj_t* feed_col = lv_obj_create(cont);
    lv_obj_set_size(feed_col, 255, lv_pct(100));
    lv_obj_set_flex_flow(feed_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(feed_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(feed_col, 5, 0);
    lv_obj_set_style_pad_row(feed_col, 8, 0);
    lv_obj_set_style_border_width(feed_col, 0, 0);
    lv_obj_set_style_bg_opa(feed_col, LV_OPA_TRANSP, 0);
    
    lv_obj_t* feed_header = lv_label_create(feed_col);
    lv_label_set_text(feed_header, "FEED");
    lv_obj_set_style_text_font(feed_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(feed_header, UITheme::TEXT_DISABLED, 0);
    
    lbl_feed_value = lv_label_create(feed_col);
    lv_label_set_text(lbl_feed_value, "100%");
    lv_obj_set_style_text_font(lbl_feed_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_feed_value, UITheme::ACCENT_SECONDARY, 0);
    
    // Reset button (distinctive color)
    lv_obj_t* feed_reset_btn = lv_button_create(feed_col);
    lv_obj_set_size(feed_reset_btn, 150, 45);
    lv_obj_set_style_bg_color(feed_reset_btn, UITheme::BTN_CONNECT, 0);
    lv_obj_add_event_cb(feed_reset_btn, feed_reset_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* feed_reset_lbl = lv_label_create(feed_reset_btn);
    lv_label_set_text(feed_reset_lbl, "100%");
    lv_obj_set_style_text_font(feed_reset_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(feed_reset_lbl);
    
    // +10% button
    lv_obj_t* feed_plus10_btn = lv_button_create(feed_col);
    lv_obj_set_size(feed_plus10_btn, 150, 45);
    lv_obj_set_style_bg_color(feed_plus10_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(feed_plus10_btn, feed_coarse_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* feed_plus10_lbl = lv_label_create(feed_plus10_btn);
    lv_label_set_text(feed_plus10_lbl, "+10%");
    lv_obj_set_style_text_font(feed_plus10_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(feed_plus10_lbl);
    
    // +1% button
    lv_obj_t* feed_plus1_btn = lv_button_create(feed_col);
    lv_obj_set_size(feed_plus1_btn, 150, 45);
    lv_obj_set_style_bg_color(feed_plus1_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(feed_plus1_btn, feed_fine_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* feed_plus1_lbl = lv_label_create(feed_plus1_btn);
    lv_label_set_text(feed_plus1_lbl, "+1%");
    lv_obj_set_style_text_font(feed_plus1_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(feed_plus1_lbl);
    
    // -1% button
    lv_obj_t* feed_minus1_btn = lv_button_create(feed_col);
    lv_obj_set_size(feed_minus1_btn, 150, 45);
    lv_obj_set_style_bg_color(feed_minus1_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(feed_minus1_btn, feed_fine_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* feed_minus1_lbl = lv_label_create(feed_minus1_btn);
    lv_label_set_text(feed_minus1_lbl, "-1%");
    lv_obj_set_style_text_font(feed_minus1_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(feed_minus1_lbl);
    
    // -10% button
    lv_obj_t* feed_minus10_btn = lv_button_create(feed_col);
    lv_obj_set_size(feed_minus10_btn, 150, 45);
    lv_obj_set_style_bg_color(feed_minus10_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(feed_minus10_btn, feed_coarse_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* feed_minus10_lbl = lv_label_create(feed_minus10_btn);
    lv_label_set_text(feed_minus10_lbl, "-10%");
    lv_obj_set_style_text_font(feed_minus10_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(feed_minus10_lbl);
    
    // === COLUMN 2: RAPID OVERRIDE ===
    lv_obj_t* rapid_col = lv_obj_create(cont);
    lv_obj_set_size(rapid_col, 255, lv_pct(100));
    lv_obj_set_flex_flow(rapid_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rapid_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(rapid_col, 5, 0);
    lv_obj_set_style_pad_row(rapid_col, 8, 0);
    lv_obj_set_style_border_width(rapid_col, 0, 0);
    lv_obj_set_style_bg_opa(rapid_col, LV_OPA_TRANSP, 0);
    
    lv_obj_t* rapid_header = lv_label_create(rapid_col);
    lv_label_set_text(rapid_header, "RAPID");
    lv_obj_set_style_text_font(rapid_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(rapid_header, UITheme::TEXT_DISABLED, 0);
    
    lbl_rapid_value = lv_label_create(rapid_col);
    lv_label_set_text(lbl_rapid_value, "100%");
    lv_obj_set_style_text_font(lbl_rapid_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_rapid_value, UITheme::ACCENT_SECONDARY, 0);
    
    // 100% button
    lv_obj_t* rapid_100_btn = lv_button_create(rapid_col);
    lv_obj_set_size(rapid_100_btn, 150, 45);
    lv_obj_set_style_bg_color(rapid_100_btn, UITheme::BTN_CONNECT, 0);
    lv_obj_add_event_cb(rapid_100_btn, rapid_100_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rapid_100_lbl = lv_label_create(rapid_100_btn);
    lv_label_set_text(rapid_100_lbl, "100%");
    lv_obj_set_style_text_font(rapid_100_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(rapid_100_lbl);
    
    // 50% button
    lv_obj_t* rapid_50_btn = lv_button_create(rapid_col);
    lv_obj_set_size(rapid_50_btn, 150, 45);
    lv_obj_set_style_bg_color(rapid_50_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(rapid_50_btn, rapid_50_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rapid_50_lbl = lv_label_create(rapid_50_btn);
    lv_label_set_text(rapid_50_lbl, "50%");
    lv_obj_set_style_text_font(rapid_50_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(rapid_50_lbl);
    
    // 25% button
    lv_obj_t* rapid_25_btn = lv_button_create(rapid_col);
    lv_obj_set_size(rapid_25_btn, 150, 45);
    lv_obj_set_style_bg_color(rapid_25_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(rapid_25_btn, rapid_25_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* rapid_25_lbl = lv_label_create(rapid_25_btn);
    lv_label_set_text(rapid_25_lbl, "25%");
    lv_obj_set_style_text_font(rapid_25_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(rapid_25_lbl);
    
    // === COLUMN 3: SPINDLE OVERRIDE ===
    lv_obj_t* spindle_col = lv_obj_create(cont);
    lv_obj_set_size(spindle_col, 255, lv_pct(100));
    lv_obj_set_flex_flow(spindle_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(spindle_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(spindle_col, 5, 0);
    lv_obj_set_style_pad_row(spindle_col, 8, 0);
    lv_obj_set_style_border_width(spindle_col, 0, 0);
    lv_obj_set_style_bg_opa(spindle_col, LV_OPA_TRANSP, 0);
    
    lv_obj_t* spindle_header = lv_label_create(spindle_col);
    lv_label_set_text(spindle_header, "SPINDLE");
    lv_obj_set_style_text_font(spindle_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(spindle_header, UITheme::TEXT_DISABLED, 0);
    
    lbl_spindle_value = lv_label_create(spindle_col);
    lv_label_set_text(lbl_spindle_value, "100%");
    lv_obj_set_style_text_font(lbl_spindle_value, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_spindle_value, UITheme::ACCENT_SECONDARY, 0);
    
    // Reset button (distinctive color)
    lv_obj_t* spindle_reset_btn = lv_button_create(spindle_col);
    lv_obj_set_size(spindle_reset_btn, 150, 45);
    lv_obj_set_style_bg_color(spindle_reset_btn, UITheme::BTN_CONNECT, 0);
    lv_obj_add_event_cb(spindle_reset_btn, spindle_reset_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* spindle_reset_lbl = lv_label_create(spindle_reset_btn);
    lv_label_set_text(spindle_reset_lbl, "100%");
    lv_obj_set_style_text_font(spindle_reset_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(spindle_reset_lbl);
    
    // +10% button
    lv_obj_t* spindle_plus10_btn = lv_button_create(spindle_col);
    lv_obj_set_size(spindle_plus10_btn, 150, 45);
    lv_obj_set_style_bg_color(spindle_plus10_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(spindle_plus10_btn, spindle_coarse_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* spindle_plus10_lbl = lv_label_create(spindle_plus10_btn);
    lv_label_set_text(spindle_plus10_lbl, "+10%");
    lv_obj_set_style_text_font(spindle_plus10_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(spindle_plus10_lbl);
    
    // +1% button
    lv_obj_t* spindle_plus1_btn = lv_button_create(spindle_col);
    lv_obj_set_size(spindle_plus1_btn, 150, 45);
    lv_obj_set_style_bg_color(spindle_plus1_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(spindle_plus1_btn, spindle_fine_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* spindle_plus1_lbl = lv_label_create(spindle_plus1_btn);
    lv_label_set_text(spindle_plus1_lbl, "+1%");
    lv_obj_set_style_text_font(spindle_plus1_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(spindle_plus1_lbl);
    
    // -1% button
    lv_obj_t* spindle_minus1_btn = lv_button_create(spindle_col);
    lv_obj_set_size(spindle_minus1_btn, 150, 45);
    lv_obj_set_style_bg_color(spindle_minus1_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(spindle_minus1_btn, spindle_fine_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* spindle_minus1_lbl = lv_label_create(spindle_minus1_btn);
    lv_label_set_text(spindle_minus1_lbl, "-1%");
    lv_obj_set_style_text_font(spindle_minus1_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(spindle_minus1_lbl);
    
    // -10% button
    lv_obj_t* spindle_minus10_btn = lv_button_create(spindle_col);
    lv_obj_set_size(spindle_minus10_btn, 150, 45);
    lv_obj_set_style_bg_color(spindle_minus10_btn, UITheme::TEXT_DARK, 0);
    lv_obj_add_event_cb(spindle_minus10_btn, spindle_coarse_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* spindle_minus10_lbl = lv_label_create(spindle_minus10_btn);
    lv_label_set_text(spindle_minus10_lbl, "-10%");
    lv_obj_set_style_text_font(spindle_minus10_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(spindle_minus10_lbl);
    
    return cont;
}

void UITabControlOverride::updateValues(float feed_ovr, float rapid_ovr, float spindle_ovr) {
    char buf[16];
    
    if (lbl_feed_value) {
        snprintf(buf, sizeof(buf), "%.0f%%", feed_ovr);
        lv_label_set_text(lbl_feed_value, buf);
    }
    if (lbl_rapid_value) {
        snprintf(buf, sizeof(buf), "%.0f%%", rapid_ovr);
        lv_label_set_text(lbl_rapid_value, buf);
    }
    if (lbl_spindle_value) {
        snprintf(buf, sizeof(buf), "%.0f%%", spindle_ovr);
        lv_label_set_text(lbl_spindle_value, buf);
    }
}
