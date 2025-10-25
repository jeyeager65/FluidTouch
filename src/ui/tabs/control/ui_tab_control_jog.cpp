#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/ui_theme.h"
#include "fluidnc_client.h"
#include <Arduino.h>

// Static member initialization
lv_obj_t *UITabControlJog::xy_step_display_label = nullptr;
lv_obj_t *UITabControlJog::z_step_display_label = nullptr;
lv_obj_t *UITabControlJog::xy_step_buttons[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::z_step_buttons[3] = {nullptr, nullptr, nullptr};
lv_obj_t *UITabControlJog::xy_feedrate_label = nullptr;
lv_obj_t *UITabControlJog::z_feedrate_label = nullptr;
float UITabControlJog::xy_current_step = 10.0f;     // Default 10mm
float UITabControlJog::z_current_step = 1.0f;       // Default 1mm
int UITabControlJog::xy_current_step_index = 2;     // Default to index 2 (10mm)
int UITabControlJog::z_current_step_index = 1;      // Default to index 1 (1mm)

void UITabControlJog::create(lv_obj_t *tab) {
    // Calculate available height - Control tab content area is ~370px
    
    // ========== XY Section (Left side) ==========
    
    // XY Step size selection - VERTICAL buttons on left
    lv_obj_t *xy_step_label = lv_label_create(tab);
    lv_label_set_text(xy_step_label, "XY Step");
    lv_obj_set_style_text_font(xy_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(xy_step_label, 5, 5);
    
    // XY Step size buttons - vertical (0.1, 1, 10, 50, 100, 500)
    static const char *xy_step_labels[] = {"0.1", "1", "10", "50", "100", "500"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, 45, 45);
        lv_obj_set_pos(btn_step, 10, 30 + i * 50);
        lv_obj_add_event_cb(btn_step, xy_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        xy_step_buttons[i] = btn_step;
        
        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, xy_step_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
    }
    update_xy_step_button_styles();
    
    // XY Jog pad (3x3 button grid) - next to step buttons
    static const char *xy_labels[] = {
        LV_SYMBOL_UP,       // NW (up, rotated -45°)
        LV_SYMBOL_UP,       // N (up)
        LV_SYMBOL_UP,       // NE (up, rotated +45°)
        LV_SYMBOL_LEFT,     // W (left)
        "",                 // Center - will show step value
        LV_SYMBOL_RIGHT,    // E (right)
        LV_SYMBOL_DOWN,     // SW (down, rotated +45°)
        LV_SYMBOL_DOWN,     // S (down)
        LV_SYMBOL_DOWN      // SE (down, rotated -45°)
    };
    
    static const int16_t xy_rotations[] = {
        -450, 0, 450,  // Top row
        0, 0, 0,       // Middle row
        450, 0, -450   // Bottom row
    };
    
    // Axis colors for direction buttons: 0=NW, 1=N, 2=NE, 3=W, 4=center, 5=E, 6=SW, 7=S, 8=SE
    static const lv_color_t xy_axis_colors[] = {
        UITheme::AXIS_XY,  // NW - diagonal
        UITheme::AXIS_Y,   // N - Y axis
        UITheme::AXIS_XY,  // NE - diagonal
        UITheme::AXIS_X,   // W - X axis
        UITheme::BG_DARKER,// Center - display only
        UITheme::AXIS_X,   // E - X axis
        UITheme::AXIS_XY,  // SW - diagonal
        UITheme::AXIS_Y,   // S - Y axis
        UITheme::AXIS_XY   // SE - diagonal
    };
    
    for (int i = 0; i < 9; i++) {
        lv_obj_t *btn_xy = lv_button_create(tab);
        lv_obj_set_size(btn_xy, 70, 70);
        lv_obj_set_pos(btn_xy, 85 + (i % 3) * 80, 30 + (i / 3) * 80);
        
        // Apply axis-specific colors
        lv_obj_set_style_bg_color(btn_xy, xy_axis_colors[i], 0);
        
        // Special styling for center button (step display)
        if (i == 4) {
            lv_obj_clear_flag(btn_xy, LV_OBJ_FLAG_CLICKABLE);
        } else {
            // Add click event for jog buttons (all except center)
            lv_obj_add_event_cb(btn_xy, xy_jog_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        }
        
        lv_obj_t *lbl = lv_label_create(btn_xy);
        lv_label_set_text(lbl, xy_labels[i]);
        
        if (i == 4) {
            xy_step_display_label = lbl;
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
            // Don't update yet - xy_feedrate_label hasn't been created
        } else {
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, 0);
        }
        
        if (xy_rotations[i] != 0) {
            lv_obj_set_style_transform_angle(lbl, xy_rotations[i], 0);
            lv_obj_set_style_transform_pivot_x(lbl, lv_pct(50), 0);
            lv_obj_set_style_transform_pivot_y(lbl, lv_pct(50), 0);
        }
        
        lv_obj_center(lbl);
    }
    
    // XY Feed rate control
    lv_obj_t *xy_feed_label = lv_label_create(tab);
    lv_label_set_text(xy_feed_label, "XY Feed:");
    lv_obj_set_style_text_font(xy_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(xy_feed_label, 85, 280);
    
    // XY Feedrate value (plain text label)
    xy_feedrate_label = lv_label_create(tab);
    lv_label_set_text(xy_feedrate_label, "3000");
    lv_obj_set_style_text_font(xy_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(xy_feedrate_label, 155, 280);
    
    // Now update XY step display (after feedrate label exists)
    update_xy_step_display();
    
    lv_obj_t *xy_feed_unit = lv_label_create(tab);
    lv_label_set_text(xy_feed_unit, "mm/min");
    lv_obj_set_style_text_font(xy_feed_unit, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(xy_feed_unit, 205, 283);
    
    // XY Feedrate adjustment buttons - all on one line: -1000, -100, +100, +1000
    lv_obj_t *btn_xy_minus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus1000, 55, 45);
    lv_obj_set_pos(btn_xy_minus1000, 85, 300);
    lv_obj_add_event_cb(btn_xy_minus1000, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1000);
    lv_obj_t *lbl_xy_minus1000 = lv_label_create(btn_xy_minus1000);
    lv_label_set_text(lbl_xy_minus1000, "-1000");
    lv_obj_set_style_text_font(lbl_xy_minus1000, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_xy_minus1000);
    
    lv_obj_t *btn_xy_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_minus100, 55, 45);
    lv_obj_set_pos(btn_xy_minus100, 145, 300);
    lv_obj_add_event_cb(btn_xy_minus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_xy_minus100 = lv_label_create(btn_xy_minus100);
    lv_label_set_text(lbl_xy_minus100, "-100");
    lv_obj_set_style_text_font(lbl_xy_minus100, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_xy_minus100);
    
    lv_obj_t *btn_xy_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus100, 55, 45);
    lv_obj_set_pos(btn_xy_plus100, 205, 300);
    lv_obj_add_event_cb(btn_xy_plus100, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_xy_plus100 = lv_label_create(btn_xy_plus100);
    lv_label_set_text(lbl_xy_plus100, "+100");
    lv_obj_set_style_text_font(lbl_xy_plus100, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_xy_plus100);
    
    lv_obj_t *btn_xy_plus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_xy_plus1000, 55, 45);
    lv_obj_set_pos(btn_xy_plus1000, 265, 300);
    lv_obj_add_event_cb(btn_xy_plus1000, xy_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1000);
    lv_obj_t *lbl_xy_plus1000 = lv_label_create(btn_xy_plus1000);
    lv_label_set_text(lbl_xy_plus1000, "+1000");
    lv_obj_set_style_text_font(lbl_xy_plus1000, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_xy_plus1000);
    
    // ========== Z Section (Right side) ==========
    
    // Z Step size selection - VERTICAL buttons on left of Z controls
    lv_obj_t *z_step_label = lv_label_create(tab);
    lv_label_set_text(z_step_label, "Z Step");
    lv_obj_set_style_text_font(z_step_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_step_label, 395, 5);
    
    // Z Step size buttons - vertical (0.1, 1, 10)
    static const char *z_step_labels[] = {"0.1", "1", "10"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, 45, 45);
        lv_obj_set_pos(btn_step, 395, 30 + i * 50);
        lv_obj_add_event_cb(btn_step, z_step_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        z_step_buttons[i] = btn_step;
        
        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, z_step_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
    }
    update_z_step_button_styles();
    
    // Z+ button - same size as XY buttons (70x70)
    lv_obj_t *btn_z_up = lv_button_create(tab);
    lv_obj_set_size(btn_z_up, 70, 70);
    lv_obj_set_pos(btn_z_up, 460, 30);
    lv_obj_set_style_bg_color(btn_z_up, UITheme::AXIS_Z, 0);
    lv_obj_add_event_cb(btn_z_up, z_jog_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);  // +Z
    lv_obj_t *lbl_z_up = lv_label_create(btn_z_up);
    lv_label_set_text(lbl_z_up, LV_SYMBOL_UP);
    lv_obj_set_style_text_font(lbl_z_up, &lv_font_montserrat_32, 0);
    lv_obj_center(lbl_z_up);
    
    // Z step display (between up/down buttons)
    lv_obj_t *z_step_display_bg = lv_obj_create(tab);
    lv_obj_set_size(z_step_display_bg, 70, 70);
    lv_obj_set_pos(z_step_display_bg, 460, 110);
    lv_obj_set_style_bg_color(z_step_display_bg, UITheme::BG_DARKER, 0);
    lv_obj_set_style_border_width(z_step_display_bg, 0, 0);
    lv_obj_clear_flag(z_step_display_bg, LV_OBJ_FLAG_SCROLLABLE);
    
    z_step_display_label = lv_label_create(z_step_display_bg);
    lv_obj_set_style_text_font(z_step_display_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(z_step_display_label, LV_TEXT_ALIGN_CENTER, 0);
    // Don't update yet - z_feedrate_label hasn't been created
    lv_obj_center(z_step_display_label);
    
    // Z- button - same size as XY buttons (70x70)
    lv_obj_t *btn_z_down = lv_button_create(tab);
    lv_obj_set_size(btn_z_down, 70, 70);
    lv_obj_set_pos(btn_z_down, 460, 190);
    lv_obj_set_style_bg_color(btn_z_down, UITheme::AXIS_Z, 0);
    lv_obj_add_event_cb(btn_z_down, z_jog_button_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1);  // -Z
    lv_obj_t *lbl_z_down = lv_label_create(btn_z_down);
    lv_label_set_text(lbl_z_down, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl_z_down, &lv_font_montserrat_32, 0);
    lv_obj_center(lbl_z_down);
    
    // Z Feed rate control
    lv_obj_t *z_feed_label = lv_label_create(tab);
    lv_label_set_text(z_feed_label, "Z Feed:");
    lv_obj_set_style_text_font(z_feed_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_feed_label, 395, 280);
    
    // Z Feedrate value (plain text label)
    z_feedrate_label = lv_label_create(tab);
    lv_label_set_text(z_feedrate_label, "1000");
    lv_obj_set_style_text_font(z_feedrate_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(z_feedrate_label, 460, 280);
    
    // Now update Z step display (after feedrate label exists)
    update_z_step_display();
    
    lv_obj_t *z_feed_unit = lv_label_create(tab);
    lv_label_set_text(z_feed_unit, "mm/min");
    lv_obj_set_style_text_font(z_feed_unit, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(z_feed_unit, 505, 283);
    
    // Z Feedrate adjustment buttons - all on one line: -1000, -100, +100, +1000
    lv_obj_t *btn_z_minus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus1000, 55, 45);
    lv_obj_set_pos(btn_z_minus1000, 395, 300);
    lv_obj_add_event_cb(btn_z_minus1000, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-1000);
    lv_obj_t *lbl_z_minus1000 = lv_label_create(btn_z_minus1000);
    lv_label_set_text(lbl_z_minus1000, "-1000");
    lv_obj_set_style_text_font(lbl_z_minus1000, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_z_minus1000);
    
    lv_obj_t *btn_z_minus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_minus100, 55, 45);
    lv_obj_set_pos(btn_z_minus100, 455, 300);
    lv_obj_add_event_cb(btn_z_minus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)-100);
    lv_obj_t *lbl_z_minus100 = lv_label_create(btn_z_minus100);
    lv_label_set_text(lbl_z_minus100, "-100");
    lv_obj_set_style_text_font(lbl_z_minus100, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_z_minus100);
    
    lv_obj_t *btn_z_plus100 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus100, 55, 45);
    lv_obj_set_pos(btn_z_plus100, 515, 300);
    lv_obj_add_event_cb(btn_z_plus100, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)100);
    lv_obj_t *lbl_z_plus100 = lv_label_create(btn_z_plus100);
    lv_label_set_text(lbl_z_plus100, "+100");
    lv_obj_set_style_text_font(lbl_z_plus100, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_z_plus100);
    
    lv_obj_t *btn_z_plus1000 = lv_button_create(tab);
    lv_obj_set_size(btn_z_plus1000, 55, 45);
    lv_obj_set_pos(btn_z_plus1000, 575, 300);
    lv_obj_add_event_cb(btn_z_plus1000, z_feedrate_adj_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1000);
    lv_obj_t *lbl_z_plus1000 = lv_label_create(btn_z_plus1000);
    lv_label_set_text(lbl_z_plus1000, "+1000");
    lv_obj_set_style_text_font(lbl_z_plus1000, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl_z_plus1000);
    
    // ========== Cancel Jog Button (Upper Right) ==========
    // Create a container for the octagon stop button
    lv_obj_t *btn_cancel = lv_obj_create(tab);
    lv_obj_set_size(btn_cancel, 70, 70);
    lv_obj_set_pos(btn_cancel, 560, 110);  // Aligned with middle row (left/right buttons)
    lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_cancel, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    
    // Add draw event to render octagon shape
    lv_obj_add_event_cb(btn_cancel, draw_octagon_event_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(btn_cancel, cancel_jog_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_cancel, LV_OBJ_FLAG_CLICKABLE);
    
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "STOP");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);
    lv_obj_center(lbl_cancel);
}

// XY Step button event handler
void UITabControlJog::xy_step_button_event_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    static const float step_values[] = {0.1f, 1.0f, 10.0f, 50.0f, 100.0f, 500.0f};
    
    xy_current_step_index = index;
    xy_current_step = step_values[index];
    update_xy_step_display();
    update_xy_step_button_styles();
    
    Serial.printf("XY Step size changed to: %.1f\n", xy_current_step);
}

// Z Step button event handler
void UITabControlJog::z_step_button_event_cb(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    static const float step_values[] = {0.1f, 1.0f, 10.0f};
    
    z_current_step_index = index;
    z_current_step = step_values[index];
    update_z_step_display();
    update_z_step_button_styles();
    
    Serial.printf("Z Step size changed to: %.1f\n", z_current_step);
}

// Update the XY step display in the center button
void UITabControlJog::update_xy_step_display() {
    if (xy_step_display_label != nullptr && xy_feedrate_label != nullptr) {
        const char *feedrate_text = lv_label_get_text(xy_feedrate_label);
        char buf[32];
        if (xy_current_step < 1.0f) {
            snprintf(buf, sizeof(buf), "S:%.1f\nF:%s", xy_current_step, feedrate_text);
        } else {
            snprintf(buf, sizeof(buf), "S:%.0f\nF:%s", xy_current_step, feedrate_text);
        }
        lv_label_set_text(xy_step_display_label, buf);
    }
}

// Update the Z step display between up/down buttons
void UITabControlJog::update_z_step_display() {
    if (z_step_display_label != nullptr && z_feedrate_label != nullptr) {
        const char *feedrate_text = lv_label_get_text(z_feedrate_label);
        char buf[32];
        if (z_current_step < 1.0f) {
            snprintf(buf, sizeof(buf), "S:%.1f\nF:%s", z_current_step, feedrate_text);
        } else {
            snprintf(buf, sizeof(buf), "S:%.0f\nF:%s", z_current_step, feedrate_text);
        }
        lv_label_set_text(z_step_display_label, buf);
    }
}

// Update XY button styles to highlight the selected step
void UITabControlJog::update_xy_step_button_styles() {
    for (int i = 0; i < 6; i++) {
        if (xy_step_buttons[i] != nullptr) {
            if (i == xy_current_step_index) {
                lv_obj_set_style_bg_color(xy_step_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(xy_step_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
            } else {
                lv_obj_set_style_bg_color(xy_step_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(xy_step_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
            }
        }
    }
}

// Update Z button styles to highlight the selected step
void UITabControlJog::update_z_step_button_styles() {
    for (int i = 0; i < 3; i++) {
        if (z_step_buttons[i] != nullptr) {
            if (i == z_current_step_index) {
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::ACCENT_PRIMARY, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::ACCENT_PRIMARY_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
            } else {
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::BG_BUTTON, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(z_step_buttons[i], UITheme::BORDER_LIGHT, LV_PART_MAIN | LV_STATE_PRESSED);
            }
        }
    }
}

// XY Feedrate adjustment button event handler
void UITabControlJog::xy_feedrate_adj_event_cb(lv_event_t *e) {
    int adjustment = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (xy_feedrate_label != nullptr) {
        const char *current_text = lv_label_get_text(xy_feedrate_label);
        int current_value = atoi(current_text);
        int new_value = current_value + adjustment;
        
        // Clamp to reasonable range (100-10000 mm/min)
        if (new_value < 100) new_value = 100;
        if (new_value > 10000) new_value = 10000;
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", new_value);
        lv_label_set_text(xy_feedrate_label, buf);
        
        // Update the center display to show new feedrate
        update_xy_step_display();
        
        Serial.printf("XY Feedrate adjusted by %d to: %d mm/min\n", adjustment, new_value);
    }
}

// Z Feedrate adjustment button event handler
void UITabControlJog::z_feedrate_adj_event_cb(lv_event_t *e) {
    int adjustment = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (z_feedrate_label != nullptr) {
        const char *current_text = lv_label_get_text(z_feedrate_label);
        int current_value = atoi(current_text);
        int new_value = current_value + adjustment;
        
        // Clamp to reasonable range (50-5000 mm/min)
        if (new_value < 50) new_value = 50;
        if (new_value > 5000) new_value = 5000;
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", new_value);
        lv_label_set_text(z_feedrate_label, buf);
        
        // Update the center display to show new feedrate
        update_z_step_display();
        
        Serial.printf("Z Feedrate adjusted by %d to: %d mm/min\n", adjustment, new_value);
    }
}

// XY Jog button event handler
void UITabControlJog::xy_jog_button_event_cb(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }
    
    int button_index = (int)(intptr_t)lv_event_get_user_data(e);
    
    // Get current feedrate
    const char *feedrate_text = lv_label_get_text(xy_feedrate_label);
    int feedrate = atoi(feedrate_text);
    
    // Map button index to X/Y movement
    // 0=NW, 1=N, 2=NE, 3=W, 4=center(skip), 5=E, 6=SW, 7=S, 8=SE
    float x_move = 0, y_move = 0;
    
    switch (button_index) {
        case 0:  // NW (-X, +Y)
            x_move = -xy_current_step;
            y_move = xy_current_step;
            break;
        case 1:  // N (+Y)
            y_move = xy_current_step;
            break;
        case 2:  // NE (+X, +Y)
            x_move = xy_current_step;
            y_move = xy_current_step;
            break;
        case 3:  // W (-X)
            x_move = -xy_current_step;
            break;
        case 5:  // E (+X)
            x_move = xy_current_step;
            break;
        case 6:  // SW (-X, -Y)
            x_move = -xy_current_step;
            y_move = -xy_current_step;
            break;
        case 7:  // S (-Y)
            y_move = -xy_current_step;
            break;
        case 8:  // SE (+X, -Y)
            x_move = xy_current_step;
            y_move = -xy_current_step;
            break;
    }
    
    // Build jog command: $J=G91 X[x] Y[y] F[feedrate]
    char jog_cmd[64];
    if (x_move != 0 && y_move != 0) {
        // Diagonal move
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 X%.3f Y%.3f F%d\n", x_move, y_move, feedrate);
    } else if (x_move != 0) {
        // X only
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 X%.3f F%d\n", x_move, feedrate);
    } else if (y_move != 0) {
        // Y only
        snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 Y%.3f F%d\n", y_move, feedrate);
    }
    
    Serial.printf("[Jog] XY Jog: %s", jog_cmd);
    FluidNCClient::sendCommand(jog_cmd);
}

// Z Jog button event handler
void UITabControlJog::z_jog_button_event_cb(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }
    
    int direction = (int)(intptr_t)lv_event_get_user_data(e);  // +1 for up, -1 for down
    
    // Get current feedrate
    const char *feedrate_text = lv_label_get_text(z_feedrate_label);
    int feedrate = atoi(feedrate_text);
    
    // Calculate Z movement
    float z_move = z_current_step * direction;
    
    // Build jog command: $J=G91 Z[z] F[feedrate]
    char jog_cmd[64];
    snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 Z%.3f F%d\n", z_move, feedrate);
    
    Serial.printf("[Jog] Z Jog: %s", jog_cmd);
    FluidNCClient::sendCommand(jog_cmd);
}

// Draw octagon shape for stop button
void UITabControlJog::draw_octagon_event_cb(lv_event_t *e) {
    lv_obj_t *obj = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    
    // Get object coordinates
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    
    int size = 70;
    int border_width = 3;
    
    // Draw outer white octagon (border)
    int cut_outer = 20;
    lv_draw_line_dsc_t white_line_dsc;
    lv_draw_line_dsc_init(&white_line_dsc);
    white_line_dsc.color = lv_color_white();
    white_line_dsc.width = 1;
    white_line_dsc.opa = LV_OPA_COVER;
    
    for (int y = 0; y < size; y++) {
        int x_start = -1, x_end = -1;
        
        if (y < cut_outer) {
            x_start = cut_outer - y;
            x_end = size - cut_outer + y;
        } else if (y < size - cut_outer) {
            x_start = 0;
            x_end = size;
        } else {
            x_start = y - (size - cut_outer);
            x_end = size - (y - (size - cut_outer));
        }
        
        if (x_start >= 0 && x_end >= 0) {
            white_line_dsc.p1.x = coords.x1 + x_start;
            white_line_dsc.p1.y = coords.y1 + y;
            white_line_dsc.p2.x = coords.x1 + x_end - 1;
            white_line_dsc.p2.y = coords.y1 + y;
            lv_draw_line(layer, &white_line_dsc);
        }
    }
    
    // Draw inner red octagon (smaller by border_width on all sides)
    int cut_inner = cut_outer - border_width;
    int offset = border_width;
    int inner_size = size - (border_width * 2);
    
    lv_draw_line_dsc_t red_line_dsc;
    lv_draw_line_dsc_init(&red_line_dsc);
    red_line_dsc.color = lv_color_hex(0xCC0000);  // Red like stop sign
    red_line_dsc.width = 1;
    red_line_dsc.opa = LV_OPA_COVER;
    
    for (int y = 0; y < inner_size; y++) {
        int x_start = -1, x_end = -1;
        
        if (y < cut_inner) {
            x_start = cut_inner - y;
            x_end = inner_size - cut_inner + y;
        } else if (y < inner_size - cut_inner) {
            x_start = 0;
            x_end = inner_size;
        } else {
            x_start = y - (inner_size - cut_inner);
            x_end = inner_size - (y - (inner_size - cut_inner));
        }
        
        if (x_start >= 0 && x_end >= 0) {
            red_line_dsc.p1.x = coords.x1 + offset + x_start;
            red_line_dsc.p1.y = coords.y1 + offset + y;
            red_line_dsc.p2.x = coords.x1 + offset + x_end - 1;
            red_line_dsc.p2.y = coords.y1 + offset + y;
            lv_draw_line(layer, &red_line_dsc);
        }
    }
}

// Cancel Jog event handler
void UITabControlJog::cancel_jog_event_cb(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Jog] Not connected to FluidNC");
        return;
    }
    
    // Send jog cancel command (0x85 or Ctrl-U)
    FluidNCClient::sendCommand("\x85");
    Serial.println("[Jog] Cancel jog command sent");
}
