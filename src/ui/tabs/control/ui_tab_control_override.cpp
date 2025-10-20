#include "ui/tabs/control/ui_tab_control_override.h"
#include "config.h"

// Event handler for feed rate override slider
static void feed_override_event_handler(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(label, "%d%%", (int)value);
    
    // TODO: Send feed rate override command to FluidNC
    // Format: FRO:<percentage> or similar G-code command
}

// Event handler for spindle speed override slider
static void spindle_override_event_handler(lv_event_t* e) {
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t* label = (lv_obj_t*)lv_event_get_user_data(e);
    
    int32_t value = lv_slider_get_value(slider);
    lv_label_set_text_fmt(label, "%d%%", (int)value);
    
    // TODO: Send spindle speed override command to FluidNC
    // Format: SRO:<percentage> or similar G-code command
}

// Event handler for reset button
static void reset_overrides_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // Get the container and find all sliders to reset them
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* parent = lv_obj_get_parent(btn);
        
        // TODO: Reset all overrides to 100%
        // Send reset commands to FluidNC
        Serial.println("Reset all overrides to 100%");
    }
}

lv_obj_t* UITabControlOverride::create(lv_obj_t* parent) {
    // Create main container - disable scrolling
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 20, 0);
    lv_obj_set_style_pad_row(cont, 15, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    
    // --- Feed Rate Override ---
    lv_obj_t* feed_label = lv_label_create(cont);
    lv_label_set_text(feed_label, "Feed Rate Override");
    lv_obj_set_style_text_font(feed_label, &lv_font_montserrat_20, 0);
    
    lv_obj_t* feed_value_label = lv_label_create(cont);
    lv_label_set_text(feed_value_label, "100%");
    lv_obj_set_style_text_font(feed_value_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(feed_value_label, lv_color_hex(0x00AA88), 0);
    
    lv_obj_t* feed_slider = lv_slider_create(cont);
    lv_obj_set_width(feed_slider, lv_pct(85));
    lv_obj_set_height(feed_slider, 20);
    lv_slider_set_range(feed_slider, 10, 200);  // 10% to 200%
    lv_slider_set_value(feed_slider, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(feed_slider, lv_color_hex(0x00AA88), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(feed_slider, lv_color_hex(0x00AA88), LV_PART_KNOB);
    lv_obj_add_event_cb(feed_slider, feed_override_event_handler, LV_EVENT_VALUE_CHANGED, feed_value_label);
    
    // --- Spacer ---
    lv_obj_t* spacer1 = lv_obj_create(cont);
    lv_obj_set_size(spacer1, 1, 30);
    lv_obj_set_style_bg_opa(spacer1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer1, 0, 0);
    
    // --- Spindle Speed Override ---
    lv_obj_t* spindle_label = lv_label_create(cont);
    lv_label_set_text(spindle_label, "Spindle Speed Override");
    lv_obj_set_style_text_font(spindle_label, &lv_font_montserrat_20, 0);
    
    lv_obj_t* spindle_value_label = lv_label_create(cont);
    lv_label_set_text(spindle_value_label, "100%");
    lv_obj_set_style_text_font(spindle_value_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(spindle_value_label, lv_color_hex(0x00AA88), 0);
    
    lv_obj_t* spindle_slider = lv_slider_create(cont);
    lv_obj_set_width(spindle_slider, lv_pct(85));
    lv_obj_set_height(spindle_slider, 20);
    lv_slider_set_range(spindle_slider, 10, 200);  // 10% to 200%
    lv_slider_set_value(spindle_slider, 100, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(spindle_slider, lv_color_hex(0x00AA88), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(spindle_slider, lv_color_hex(0x00AA88), LV_PART_KNOB);
    lv_obj_add_event_cb(spindle_slider, spindle_override_event_handler, LV_EVENT_VALUE_CHANGED, spindle_value_label);
    
    // --- Spacer ---
    lv_obj_t* spacer2 = lv_obj_create(cont);
    lv_obj_set_size(spacer2, 1, 30);
    lv_obj_set_style_bg_opa(spacer2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer2, 0, 0);
    
    // --- Reset Button ---
    lv_obj_t* reset_btn = lv_button_create(cont);
    lv_obj_set_size(reset_btn, 220, 55);
    lv_obj_set_style_bg_color(reset_btn, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(reset_btn, reset_overrides_event_handler, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* reset_label = lv_label_create(reset_btn);
    lv_label_set_text(reset_label, "Reset to 100%");
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_18, 0);
    lv_obj_center(reset_label);
    
    return cont;
}
