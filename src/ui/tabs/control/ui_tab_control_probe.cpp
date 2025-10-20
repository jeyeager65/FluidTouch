#include "ui/tabs/control/ui_tab_control_probe.h"
#include <lvgl.h>

void UITabControlProbe::create(lv_obj_t *parent) {
    // Probe Type Selection
    lv_obj_t *probe_type_label = lv_label_create(parent);
    lv_label_set_text(probe_type_label, "Probe Type:");
    lv_obj_set_style_text_font(probe_type_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(probe_type_label, 10, 10);
    
    // Probe type dropdown
    lv_obj_t *probe_dropdown = lv_dropdown_create(parent);
    lv_dropdown_set_options(probe_dropdown, "Touch Probe\nTool Length\nCorner Finder\nEdge Finder");
    lv_obj_set_size(probe_dropdown, 250, LV_SIZE_CONTENT);
    lv_obj_set_pos(probe_dropdown, 10, 45);
    
    // Probe Direction Section
    lv_obj_t *direction_label = lv_label_create(parent);
    lv_label_set_text(direction_label, "Probe Direction:");
    lv_obj_set_style_text_font(direction_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(direction_label, 10, 100);
    
    // Direction buttons (3x3 grid for XYZ directions)
    static const char *dir_labels[] = {"-X", "X0", "+X", "-Y", "Y0", "+Y", "-Z", "Z0", "+Z"};
    lv_obj_t *dir_container = lv_obj_create(parent);
    lv_obj_set_size(dir_container, 280, 200);
    lv_obj_set_pos(dir_container, 10, 135);
    lv_obj_set_style_pad_all(dir_container, 5, 0);
    lv_obj_set_style_pad_gap(dir_container, 5, 0);
    lv_obj_set_flex_flow(dir_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_border_width(dir_container, 0, 0);
    
    for (int i = 0; i < 9; i++) {
        lv_obj_t *dir_btn = lv_button_create(dir_container);
        lv_obj_set_size(dir_btn, 85, 60);
        lv_obj_t *dir_btn_label = lv_label_create(dir_btn);
        lv_label_set_text(dir_btn_label, dir_labels[i]);
        lv_obj_center(dir_btn_label);
        lv_obj_set_style_text_font(dir_btn_label, &lv_font_montserrat_18, 0);
    }
    
    // Probe Parameters
    lv_obj_t *params_label = lv_label_create(parent);
    lv_label_set_text(params_label, "Parameters:");
    lv_obj_set_style_text_font(params_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(params_label, 310, 10);
    
    // Feed rate
    lv_obj_t *probe_feed_label = lv_label_create(parent);
    lv_label_set_text(probe_feed_label, "Feed Rate:");
    lv_obj_set_pos(probe_feed_label, 310, 50);
    
    lv_obj_t *probe_feed_input = lv_textarea_create(parent);
    lv_textarea_set_one_line(probe_feed_input, true);
    lv_textarea_set_text(probe_feed_input, "100");
    lv_obj_set_size(probe_feed_input, 100, LV_SIZE_CONTENT);
    lv_obj_set_pos(probe_feed_input, 410, 45);
    
    lv_obj_t *probe_feed_unit = lv_label_create(parent);
    lv_label_set_text(probe_feed_unit, "mm/min");
    lv_obj_set_pos(probe_feed_unit, 520, 50);
    
    // Max distance
    lv_obj_t *dist_label = lv_label_create(parent);
    lv_label_set_text(dist_label, "Max Dist:");
    lv_obj_set_pos(dist_label, 310, 95);
    
    lv_obj_t *dist_input = lv_textarea_create(parent);
    lv_textarea_set_one_line(dist_input, true);
    lv_textarea_set_text(dist_input, "10");
    lv_obj_set_size(dist_input, 100, LV_SIZE_CONTENT);
    lv_obj_set_pos(dist_input, 410, 90);
    
    lv_obj_t *dist_unit = lv_label_create(parent);
    lv_label_set_text(dist_unit, "mm");
    lv_obj_set_pos(dist_unit, 520, 95);
    
    // Retract distance
    lv_obj_t *retract_label = lv_label_create(parent);
    lv_label_set_text(retract_label, "Retract:");
    lv_obj_set_pos(retract_label, 310, 140);
    
    lv_obj_t *retract_input = lv_textarea_create(parent);
    lv_textarea_set_one_line(retract_input, true);
    lv_textarea_set_text(retract_input, "2");
    lv_obj_set_size(retract_input, 100, LV_SIZE_CONTENT);
    lv_obj_set_pos(retract_input, 410, 135);
    
    lv_obj_t *retract_unit = lv_label_create(parent);
    lv_label_set_text(retract_unit, "mm");
    lv_obj_set_pos(retract_unit, 520, 140);
    
    // Start Probe button
    lv_obj_t *probe_btn = lv_button_create(parent);
    lv_obj_set_size(probe_btn, 270, 60);
    lv_obj_set_pos(probe_btn, 310, 180);
    lv_obj_set_style_bg_color(probe_btn, lv_color_hex(0x00AA00), 0);
    
    lv_obj_t *probe_btn_label = lv_label_create(probe_btn);
    lv_label_set_text(probe_btn_label, "Start Probe");
    lv_obj_center(probe_btn_label);
    lv_obj_set_style_text_font(probe_btn_label, &lv_font_montserrat_20, 0);
    
    // Results display
    lv_obj_t *results_label = lv_label_create(parent);
    lv_label_set_text(results_label, "Results:");
    lv_obj_set_style_text_font(results_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(results_label, 310, 260);
    
    lv_obj_t *results_text = lv_textarea_create(parent);
    lv_textarea_set_text(results_text, "No probe data");
    lv_obj_set_size(results_text, 270, 65);
    lv_obj_set_pos(results_text, 310, 290);
    lv_obj_set_style_bg_color(results_text, lv_color_hex(0x222222), 0);
}
