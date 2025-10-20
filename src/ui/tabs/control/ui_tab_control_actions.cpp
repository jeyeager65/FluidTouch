#include "ui/tabs/control/ui_tab_control_actions.h"

void UITabControlActions::create(lv_obj_t *tab) {
    // Zero buttons section
    lv_obj_t *zero_label = lv_label_create(tab);
    lv_label_set_text(zero_label, "Zero Axis:");
    lv_obj_set_style_text_font(zero_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(zero_label, 10, 10);
    
    static const char *zero_labels[] = {"Zero X", "Zero Y", "Zero Z", "Zero All"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn_zero = lv_button_create(tab);
        lv_obj_set_size(btn_zero, 140, 50);
        lv_obj_set_pos(btn_zero, 10 + (i % 2) * 150, 45 + (i / 2) * 60);
        lv_obj_t *lbl = lv_label_create(btn_zero);
        lv_label_set_text(lbl, zero_labels[i]);
        lv_obj_center(lbl);
    }
    
    // Home buttons section
    lv_obj_t *home_label = lv_label_create(tab);
    lv_label_set_text(home_label, "Home Axis:");
    lv_obj_set_style_text_font(home_label, &lv_font_montserrat_18, 0);
    lv_obj_set_pos(home_label, 330, 10);
    
    static const char *home_labels[] = {"Home X", "Home Y", "Home Z", "Home All"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn_home = lv_button_create(tab);
        lv_obj_set_size(btn_home, 140, 50);
        lv_obj_set_pos(btn_home, 330 + (i % 2) * 150, 45 + (i / 2) * 60);
        lv_obj_t *lbl = lv_label_create(btn_home);
        lv_label_set_text(lbl, home_labels[i]);
        lv_obj_center(lbl);
    }
    
    // Control buttons - row 1
    lv_obj_t *btn_pause = lv_button_create(tab);
    lv_obj_set_size(btn_pause, 200, 60);
    lv_obj_set_pos(btn_pause, 10, 180);
    lv_obj_t *lbl_pause = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause, "Pause");
    lv_obj_set_style_text_font(lbl_pause, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_pause);
    
    lv_obj_t *btn_unlock = lv_button_create(tab);
    lv_obj_set_size(btn_unlock, 200, 60);
    lv_obj_set_pos(btn_unlock, 220, 180);
    lv_obj_t *lbl_unlock = lv_label_create(btn_unlock);
    lv_label_set_text(lbl_unlock, "Unlock");
    lv_obj_set_style_text_font(lbl_unlock, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_unlock);
    
    lv_obj_t *btn_reset = lv_button_create(tab);
    lv_obj_set_size(btn_reset, 200, 60);
    lv_obj_set_pos(btn_reset, 430, 180);
    lv_obj_t *lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Soft Reset");
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_reset);

    // Quick Stop button - full width
    lv_obj_t *btn_estop = lv_button_create(tab);
    lv_obj_set_size(btn_estop, 620, 60);
    lv_obj_set_pos(btn_estop, 10, 260);
    lv_obj_set_style_bg_color(btn_estop, lv_color_hex(0xB71C1C), LV_PART_MAIN);
    lv_obj_t *lbl_estop = lv_label_create(btn_estop);
    lv_label_set_text(lbl_estop, "QUICK STOP");
    lv_obj_set_style_text_font(lbl_estop, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_estop);
}
