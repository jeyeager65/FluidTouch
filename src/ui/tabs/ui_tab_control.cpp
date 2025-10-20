#include "ui/tabs/ui_tab_control.h"
#include "ui/tabs/control/ui_tab_control_actions.h"
#include "ui/tabs/control/ui_tab_control_jog.h"
#include "ui/tabs/control/ui_tab_control_joystick.h"
#include "ui/tabs/control/ui_tab_control_probe.h"
#include "ui/tabs/control/ui_tab_control_override.h"
#include <Arduino.h>

    
void UITabControl::create(lv_obj_t *tab) {
    // Remove padding from the tab itself
    lv_obj_set_style_pad_all(tab, 0, 0);
    
    // Create main content container
    lv_obj_t *content = lv_obj_create(tab);
    lv_obj_set_size(content, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(content, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Create vertical tabview on the left side
    lv_obj_t *sub_tabview = lv_tabview_create(content);
    lv_obj_set_size(sub_tabview, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(sub_tabview, 0, 0);
    lv_tabview_set_tab_bar_position(sub_tabview, LV_DIR_LEFT);
    lv_tabview_set_tab_bar_size(sub_tabview, 150);
    
    // Disable swiping between tabs by disabling scrolling on the content
    lv_obj_t *sub_content = lv_tabview_get_content(sub_tabview);
    lv_obj_clear_flag(sub_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(sub_content, 0, 0);
    
    // Style the sub-tabview
    lv_obj_set_style_bg_color(sub_tabview, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_pad_all(sub_tabview, 0, 0);  // Remove padding from tabview
    
    // Get tab bar and remove all padding/spacing
    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(sub_tabview);
    lv_obj_set_style_pad_all(tab_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(tab_bar, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_row(tab_bar, 0, 0);  // Remove vertical spacing between buttons
    lv_obj_set_style_pad_column(tab_bar, 0, 0);  // Remove horizontal spacing
    lv_obj_set_style_text_font(tab_bar, &lv_font_montserrat_18, 0);  // Font for subtabs
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x1a1a1a), 0);  // Darker background for tab bar

    // Also ensure the content area has no padding
    lv_obj_set_style_pad_all(lv_tabview_get_content(sub_tabview), 0, 0);
    
    // Style tab buttons (inactive) - different colors than main tabs
    lv_obj_set_style_bg_color(sub_tabview, lv_color_hex(0x2a2a2a), LV_PART_ITEMS);
    lv_obj_set_style_text_color(sub_tabview, lv_color_hex(0xAAAAAA), LV_PART_ITEMS);
    lv_obj_set_style_border_width(sub_tabview, 0, LV_PART_ITEMS);
    lv_obj_set_style_text_font(sub_tabview, &lv_font_montserrat_18, LV_PART_ITEMS);
    
    // Style tab buttons (active/checked) - use a different accent color
    lv_obj_set_style_bg_color(sub_tabview, lv_color_hex(0x00AA88), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(sub_tabview, lv_color_white(), LV_PART_ITEMS | LV_STATE_CHECKED);

    
    
    // Add sub-tabs (reordered)
    lv_obj_t *tab_actions = lv_tabview_add_tab(sub_tabview, "Actions");
    lv_obj_t *tab_jog = lv_tabview_add_tab(sub_tabview, "Jog");
    lv_obj_t *tab_joystick = lv_tabview_add_tab(sub_tabview, "Joystick");
    lv_obj_t *tab_probe = lv_tabview_add_tab(sub_tabview, "Probe");
    lv_obj_t *tab_overrides = lv_tabview_add_tab(sub_tabview, "Overrides");
    
    // Disable scrolling on sub-tabs
    lv_obj_clear_flag(tab_actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_jog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_joystick, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_probe, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(tab_overrides, LV_OBJ_FLAG_SCROLLABLE);
    
    // Set background for sub-tabs
    lv_obj_set_style_bg_color(tab_actions, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(tab_jog, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(tab_joystick, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(tab_probe, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_bg_color(tab_overrides, lv_color_hex(0x2a2a2a), 0);
    
    // Add 5px padding to all sub-tabs
    lv_obj_set_style_pad_all(tab_actions, 5, 0);
    lv_obj_set_style_pad_all(tab_jog, 5, 0);
    lv_obj_set_style_pad_all(tab_joystick, 5, 0);
    lv_obj_set_style_pad_all(tab_probe, 5, 0);
    lv_obj_set_style_pad_all(tab_overrides, 5, 0);

    // Get the actual tab buttons and style them directly with the teal accent color
    uint32_t tab_count = lv_obj_get_child_count(tab_bar);
    for (uint32_t i = 0; i < tab_count; i++) {
        lv_obj_t *btn = lv_obj_get_child(tab_bar, i);
        if (btn) {
            // Inactive state
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0xAAAAAA), 0);
            
            // Checked/Active state - teal color scheme
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x00AA88), LV_STATE_CHECKED);
            lv_obj_set_style_text_color(btn, lv_color_white(), LV_STATE_CHECKED);
            
            // Set border on right side only (indicator pointing toward content)
            lv_obj_set_style_border_color(btn, lv_color_hex(0x00AA88), LV_STATE_CHECKED);
            lv_obj_set_style_border_width(btn, 0, LV_STATE_CHECKED);  // Set width to 0 first
            lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_RIGHT, LV_STATE_CHECKED);  // Right side only
            lv_obj_set_style_border_width(btn, 4, LV_STATE_CHECKED);  // Then set width to 4px
        }
    }

    // ============================================
    // Create Content for Each Sub-Tab
    // ============================================
    UITabControlActions::create(tab_actions);
    UITabControlJog::create(tab_jog);
    UITabControlJoystick::create(tab_joystick);
    UITabControlProbe::create(tab_probe);
    UITabControlOverride::create(tab_overrides);
}