#include "ui/tabs/control/ui_tab_control_jog.h"

void UITabControlJog::create(lv_obj_t *tab) {
    // Step size selection
    lv_obj_t *step_label = lv_label_create(tab);
    lv_label_set_text(step_label, "Step Size:");
    lv_obj_set_style_text_font(step_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(step_label, 10, 10);
    
    // Step size buttons
    static const char *step_labels[] = {"0.1", "1", "10", "100"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn_step = lv_button_create(tab);
        lv_obj_set_size(btn_step, 70, 50);
        lv_obj_set_pos(btn_step, 120 + i * 80, 5);
        lv_obj_t *lbl = lv_label_create(btn_step);
        lv_label_set_text(lbl, step_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_center(lbl);
    }
    
    // XY Jog pad (3x3 button grid)
    lv_obj_t *xy_label = lv_label_create(tab);
    lv_label_set_text(xy_label, "XY Jog:");
    lv_obj_set_style_text_font(xy_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(xy_label, 10, 75);
    
    static const char *xy_labels[] = {"-X-Y", "-Y", "+X-Y", "-X", "Home", "+X", "-X+Y", "+Y", "+X+Y"};
    for (int i = 0; i < 9; i++) {
        lv_obj_t *btn_xy = lv_button_create(tab);
        lv_obj_set_size(btn_xy, 80, 80);
        lv_obj_set_pos(btn_xy, 10 + (i % 3) * 90, 110 + (i / 3) * 90);
        
        // Special styling for center button (home)
        if (i == 4) {
            lv_obj_set_style_bg_color(btn_xy, lv_color_hex(0x555555), 0);
        }
        
        lv_obj_t *lbl = lv_label_create(btn_xy);
        lv_label_set_text(lbl, xy_labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(lbl);
    }
    
    // Z Jog buttons (vertical)
    lv_obj_t *z_label = lv_label_create(tab);
    lv_label_set_text(z_label, "Z Jog:");
    lv_obj_set_style_text_font(z_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(z_label, 320, 75);
    
    // Z+ button
    lv_obj_t *btn_z_up = lv_button_create(tab);
    lv_obj_set_size(btn_z_up, 100, 100);
    lv_obj_set_pos(btn_z_up, 320, 110);
    lv_obj_t *lbl_z_up = lv_label_create(btn_z_up);
    lv_label_set_text(lbl_z_up, "Z+");
    lv_obj_set_style_text_font(lbl_z_up, &lv_font_montserrat_32, 0);
    lv_obj_center(lbl_z_up);
    
    // Z- button
    lv_obj_t *btn_z_down = lv_button_create(tab);
    lv_obj_set_size(btn_z_down, 100, 100);
    lv_obj_set_pos(btn_z_down, 320, 220);
    lv_obj_t *lbl_z_down = lv_label_create(btn_z_down);
    lv_label_set_text(lbl_z_down, "Z-");
    lv_obj_set_style_text_font(lbl_z_down, &lv_font_montserrat_32, 0);
    lv_obj_center(lbl_z_down);
    
    // Feed rate control
    lv_obj_t *feed_label = lv_label_create(tab);
    lv_label_set_text(feed_label, "Feed Rate:");
    lv_obj_set_style_text_font(feed_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(feed_label, 450, 75);
    
    lv_obj_t *feed_input = lv_textarea_create(tab);
    lv_textarea_set_one_line(feed_input, true);
    lv_textarea_set_text(feed_input, "1000");
    lv_obj_set_size(feed_input, 120, LV_SIZE_CONTENT);
    lv_obj_set_pos(feed_input, 450, 110);
    
    lv_obj_t *feed_unit = lv_label_create(tab);
    lv_label_set_text(feed_unit, "mm/min");
    lv_obj_set_pos(feed_unit, 580, 115);
}
