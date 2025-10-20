#include "ui/tabs/ui_tab_files.h"

void UITabFiles::create(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x2a2a2a), LV_PART_MAIN);

    // Storage selection dropdown
    lv_obj_t *storage_label = lv_label_create(tab);
    lv_label_set_text(storage_label, "Storage:");
    lv_obj_set_style_text_font(storage_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(storage_label, lv_color_white(), 0);
    lv_obj_set_pos(storage_label, 15, 10);

    lv_obj_t *storage_dropdown = lv_dropdown_create(tab);
    lv_dropdown_set_options(storage_dropdown, "SD\nFlash");
    lv_dropdown_set_selected(storage_dropdown, 0);  // Select "SD" by default (index 0)
    lv_obj_set_size(storage_dropdown, 150, 40);
    lv_obj_set_pos(storage_dropdown, 100, 5);
    lv_obj_set_style_text_font(storage_dropdown, &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(storage_dropdown, lv_color_hex(0x404040), 0);
    lv_obj_set_style_text_color(storage_dropdown, lv_color_white(), 0);
    
    // Get the dropdown list and set its height properly
    lv_obj_t *dropdown_list = lv_dropdown_get_list(storage_dropdown);
    lv_obj_set_style_max_height(dropdown_list, 100, 0);

    // Refresh button
    lv_obj_t *btn_refresh = lv_button_create(tab);
    lv_obj_set_size(btn_refresh, 100, 40);
    lv_obj_set_pos(btn_refresh, 270, 5);
    lv_obj_set_style_bg_color(btn_refresh, lv_color_hex(0x0078D7), 0);
    
    lv_obj_t *lbl_refresh = lv_label_create(btn_refresh);
    lv_label_set_text(lbl_refresh, "Refresh");
    lv_obj_set_style_text_font(lbl_refresh, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_refresh);

    // File list container with scrolling
    lv_obj_t *file_list_container = lv_obj_create(tab);
    lv_obj_set_size(file_list_container, 780, 275);
    lv_obj_set_pos(file_list_container, 10, 55);
    lv_obj_set_style_bg_color(file_list_container, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
    lv_obj_set_style_border_color(file_list_container, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_border_width(file_list_container, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(file_list_container, 5, LV_PART_MAIN);
    lv_obj_set_flex_flow(file_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(file_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(file_list_container, 5, 0);

    // Sample file entries (8 entries with scrolling)
    const char* sample_files[] = {
        "test_part.gcode",
        "calibration_square.gcode",
        "logo_engraving.gcode",
        "pcb_drill.gcode",
        "surface_leveling.gcode",
        "tool_height.gcode",
        "circle_test.gcode",
        "test_pattern.gcode"
    };

    for(int i = 0; i < 8; i++) {
        // File entry container
        lv_obj_t *file_entry = lv_obj_create(file_list_container);
        lv_obj_set_size(file_entry, 750, 55);
        lv_obj_set_style_bg_color(file_entry, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_border_color(file_entry, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_width(file_entry, 1, 0);
        lv_obj_set_style_radius(file_entry, 4, 0);
        lv_obj_set_style_pad_all(file_entry, 8, 0);
        lv_obj_clear_flag(file_entry, LV_OBJ_FLAG_SCROLLABLE);

        // File name label
        lv_obj_t *file_name = lv_label_create(file_entry);
        lv_label_set_text(file_name, sample_files[i]);
        lv_obj_set_style_text_font(file_name, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(file_name, lv_color_white(), 0);
        lv_obj_align(file_name, LV_ALIGN_LEFT_MID, 5, 0);

        // File size label
        lv_obj_t *file_size = lv_label_create(file_entry);
        lv_label_set_text(file_size, "1.2 KB");
        lv_obj_set_style_text_font(file_size, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(file_size, lv_color_hex(0x888888), 0);
        lv_obj_align(file_size, LV_ALIGN_LEFT_MID, 380, 0);

        // Play button
        lv_obj_t *btn_play = lv_button_create(file_entry);
        lv_obj_set_size(btn_play, 80, 40);
        lv_obj_align(btn_play, LV_ALIGN_RIGHT_MID, -5, 0);
        lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_radius(btn_play, 4, 0);

        lv_obj_t *lbl_play = lv_label_create(btn_play);
        lv_label_set_text(lbl_play, LV_SYMBOL_PLAY);
        lv_obj_set_style_text_font(lbl_play, &lv_font_montserrat_24, 0);
        lv_obj_center(lbl_play);
    }
}
