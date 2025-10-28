#include "ui/tabs/ui_tab_macros.h"
#include "ui/ui_theme.h"
#include <Arduino.h>

// Forward declaration for edit button event handler
static void btn_edit_event_handler(lv_event_t *e);

void UITabMacros::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);
    
    // Remove default padding on tab
    lv_obj_set_style_pad_all(tab, 0, 0);

    // Edit button (upper right corner, absolute positioning on tab)
    lv_obj_t *btn_edit = lv_btn_create(tab);
    lv_obj_set_size(btn_edit, 120, 45);
    lv_obj_set_style_bg_color(btn_edit, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_pos(btn_edit, 670, 10);  // Absolute positioning (800 - 120 - 10 = 670)
    lv_obj_add_event_cb(btn_edit, btn_edit_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t *edit_label = lv_label_create(btn_edit);
    lv_label_set_text(edit_label, LV_SYMBOL_EDIT " Edit");
    lv_obj_set_style_text_font(edit_label, &lv_font_montserrat_16, 0);
    lv_obj_center(edit_label);

    // Macro container for flex layout
    lv_obj_t *macro_container = lv_obj_create(tab);
    lv_obj_set_size(macro_container, 780, 283);
    lv_obj_set_style_bg_color(macro_container, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(macro_container, 1, 0);
    lv_obj_set_style_border_color(macro_container, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_pad_all(macro_container, 20, 0);
    lv_obj_set_style_pad_right(macro_container, 0, 0);
    lv_obj_set_pos(macro_container, 10, 65);  // Position below Edit button
    lv_obj_clear_flag(macro_container, LV_OBJ_FLAG_SCROLLABLE);

    // Enable flex layout on container for automatic flow wrapping
    lv_obj_set_layout(macro_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(macro_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(macro_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(macro_container, 15, 0);     // Vertical gap between rows
    lv_obj_set_style_pad_column(macro_container, 15, 0);  // Horizontal gap between buttons

    // Macro buttons - 9 buttons total, will flow into rows automatically
    const char* macro_default_labels[] = {
        "Tool\nChange", "Probe\nZ", "Corner\nProbe",
        "Go to\nOrigin", "Park\nPosition", "Spindle\nTest",
        "Coolant\nOn", "Coolant\nOff", "Safe\nHeight"
    };
    
    const lv_color_t macro_colors[] = {
        UITheme::MACRO_COLOR_1, UITheme::MACRO_COLOR_2, UITheme::MACRO_COLOR_4,
        UITheme::MACRO_COLOR_3, UITheme::MACRO_COLOR_6, UITheme::MACRO_COLOR_1,
        UITheme::MACRO_COLOR_5, UITheme::MACRO_COLOR_7, UITheme::MACRO_COLOR_2
    };

    int macro_btn_width = 236;
    int macro_btn_height = 72;  // Reduced by 10px (was 95)

    for (int i = 0; i < 9; i++) {
        lv_obj_t *btn_macro = lv_button_create(macro_container);  // Create in container, not tab
        lv_obj_set_size(btn_macro, macro_btn_width, macro_btn_height);
        lv_obj_set_style_bg_color(btn_macro, macro_colors[i], LV_PART_MAIN);
        
        lv_obj_t *lbl_macro = lv_label_create(btn_macro);
        lv_label_set_text(lbl_macro, macro_default_labels[i]);
        lv_obj_set_style_text_font(lbl_macro, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_align(lbl_macro, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl_macro);
    }
}

// Edit button event handler
static void btn_edit_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        Serial.println("Macros Edit button clicked - functionality to be implemented");
        // TODO: Implement macro editing functionality
    }
}
