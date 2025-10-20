#include "ui/tabs/control/ui_tab_control_joystick.h"
#include <lvgl.h>
#include <math.h>

// XY Joystick drag event handler (circular movement)
static void xy_joystick_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
    
    // Safety check: ensure object is still valid
    if (!knob || !lv_obj_is_valid(knob)) return;
    
    if (code == LV_EVENT_PRESSING) {
        lv_obj_t *bg = lv_obj_get_parent(knob);
        
        // Safety check: ensure parent is valid
        if (!bg || !lv_obj_is_valid(bg)) return;
        
        lv_indev_t *indev = lv_indev_get_act();
        
        if (indev == NULL) return;
        
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        // Get background dimensions
        int32_t bg_w = lv_obj_get_width(bg);
        int32_t bg_h = lv_obj_get_height(bg);
        
        // Get absolute position of background on screen
        lv_area_t bg_coords;
        lv_obj_get_coords(bg, &bg_coords);
        
        // Calculate center position
        lv_point_t bg_center;
        bg_center.x = (bg_coords.x1 + bg_coords.x2) / 2;
        bg_center.y = (bg_coords.y1 + bg_coords.y2) / 2;
        
        // Calculate offset from center
        int32_t dx = point.x - bg_center.x;
        int32_t dy = point.y - bg_center.y;
        
        // Calculate distance from center
        float distance = sqrt(dx * dx + dy * dy);
        
        // Limit knob to stay within circle (radius - knob_radius)
        int32_t max_radius = (bg_w / 2) - (lv_obj_get_width(knob) / 2) - 5;
        
        if (distance > max_radius) {
            // Normalize to edge of circle
            float ratio = max_radius / distance;
            dx = (int32_t)(dx * ratio);
            dy = (int32_t)(dy * ratio);
        }
        
        // Position knob relative to background center using align
        lv_obj_align(knob, LV_ALIGN_CENTER, dx, dy);
        
        // Calculate joystick percentage (-100 to +100)
        float x_percent = (dx * 100.0f) / max_radius;
        float y_percent = -(dy * 100.0f) / max_radius;  // Invert Y axis
        
        // TODO: Send jog command based on x_percent and y_percent
    }
    else if (code == LV_EVENT_RELEASED) {
        // Return knob to center when released
        lv_obj_center(knob);
        
        // TODO: Send jog cancel command
    }
}

// Z Joystick drag event handler (vertical only)
static void z_joystick_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
    
    // Safety check: ensure object is still valid
    if (!knob || !lv_obj_is_valid(knob)) return;
    
    if (code == LV_EVENT_PRESSING) {
        lv_obj_t *bg = lv_obj_get_parent(knob);
        
        // Safety check: ensure parent is valid
        if (!bg || !lv_obj_is_valid(bg)) return;
        
        lv_indev_t *indev = lv_indev_get_act();
        
        if (indev == NULL) return;
        
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        // Get background dimensions
        int32_t bg_h = lv_obj_get_height(bg);
        
        // Get absolute position of background on screen
        lv_area_t bg_coords;
        lv_obj_get_coords(bg, &bg_coords);
        
        // Calculate center Y position
        int32_t bg_center_y = (bg_coords.y1 + bg_coords.y2) / 2;
        
        // Calculate offset from center (vertical only)
        int32_t dy = point.y - bg_center_y;
        
        // Limit knob to stay within slider (max_offset from center)
        int32_t max_offset = (bg_h / 2) - (lv_obj_get_height(knob) / 2) - 5;
        
        if (dy < -max_offset) dy = -max_offset;
        if (dy > max_offset) dy = max_offset;
        
        // Position knob vertically using align (keeps centered horizontally)
        lv_obj_align(knob, LV_ALIGN_CENTER, 0, dy);
        
        // Calculate joystick percentage (-100 to +100, inverted for Z)
        float z_percent = -(dy * 100.0f) / max_offset;  // Negative dy = Z+
        
        // TODO: Send Z jog command based on z_percent
    }
    else if (code == LV_EVENT_RELEASED) {
        // Return knob to center when released
        lv_obj_center(knob);
        
        // TODO: Send jog cancel command
    }
}

void UITabControlJoystick::create(lv_obj_t *parent) {
    // Set parent to use horizontal flex layout (XY joystick on left, Z slider on right)
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);
    lv_obj_set_style_pad_gap(parent, 20, 0);

    // ========== XY Joystick (Circular) ==========
    lv_obj_t *xy_container = lv_obj_create(parent);
    lv_obj_set_size(xy_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(xy_container, 5, 0);
    lv_obj_set_flex_flow(xy_container, LV_FLEX_FLOW_COLUMN);  // Stack label and joystick vertically
    lv_obj_clear_flag(xy_container, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    
    // XY Label
    lv_obj_t *xy_label = lv_label_create(xy_container);
    lv_label_set_text(xy_label, "XY Jog");
    lv_obj_set_style_text_font(xy_label, &lv_font_montserrat_18, 0);

    // Background circle (220x220 with crosshairs)
    lv_obj_t *xy_bg = lv_obj_create(xy_container);
    lv_obj_set_size(xy_bg, 220, 220);
    lv_obj_set_style_radius(xy_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(xy_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(xy_bg, 2, 0);
    lv_obj_set_style_border_color(xy_bg, lv_color_hex(0x808080), 0);
    lv_obj_clear_flag(xy_bg, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

    // Horizontal crosshair
    lv_obj_t *xy_h_line = lv_obj_create(xy_bg);
    lv_obj_set_size(xy_h_line, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(xy_h_line, lv_color_hex(0x606060), 0);
    lv_obj_set_style_border_width(xy_h_line, 0, 0);
    lv_obj_center(xy_h_line);

    // Vertical crosshair
    lv_obj_t *xy_v_line = lv_obj_create(xy_bg);
    lv_obj_set_size(xy_v_line, 2, LV_PCT(100));
    lv_obj_set_style_bg_color(xy_v_line, lv_color_hex(0x606060), 0);
    lv_obj_set_style_border_width(xy_v_line, 0, 0);
    lv_obj_center(xy_v_line);

    // Draggable knob (90x90, blue, centered initially)
    lv_obj_t *xy_knob = lv_obj_create(xy_bg);
    lv_obj_set_size(xy_knob, 90, 90);
    lv_obj_set_style_radius(xy_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(xy_knob, lv_color_hex(0x0078D7), 0);
    lv_obj_set_style_border_width(xy_knob, 3, 0);
    lv_obj_set_style_border_color(xy_knob, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(xy_knob);
    
    // Add label to XY knob
    lv_obj_t *xy_knob_label = lv_label_create(xy_knob);
    lv_label_set_text(xy_knob_label, "XY");
    lv_obj_set_style_text_font(xy_knob_label, &lv_font_montserrat_20, 0);
    lv_obj_center(xy_knob_label);
    
    // Add drag event to knob
    lv_obj_add_flag(xy_knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(xy_knob, xy_joystick_event_handler, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(xy_knob, xy_joystick_event_handler, LV_EVENT_RELEASED, NULL);

    // ========== Z Slider (Vertical) ==========
    lv_obj_t *z_container = lv_obj_create(parent);
    lv_obj_set_size(z_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(z_container, 5, 0);
    lv_obj_set_flex_flow(z_container, LV_FLEX_FLOW_COLUMN);  // Stack label and slider vertically
    lv_obj_clear_flag(z_container, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling
    
    // Z Label
    lv_obj_t *z_label = lv_label_create(z_container);
    lv_label_set_text(z_label, "Z Jog");
    lv_obj_set_style_text_font(z_label, &lv_font_montserrat_18, 0);

    // Background slider (80x220 vertical)
    lv_obj_t *z_bg = lv_obj_create(z_container);
    lv_obj_set_size(z_bg, 80, 220);
    lv_obj_set_style_radius(z_bg, 15, 0);
    lv_obj_set_style_bg_color(z_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(z_bg, 2, 0);
    lv_obj_set_style_border_color(z_bg, lv_color_hex(0x808080), 0);
    lv_obj_clear_flag(z_bg, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

    // Center line
    lv_obj_t *z_line = lv_obj_create(z_bg);
    lv_obj_set_size(z_line, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(z_line, lv_color_hex(0x606060), 0);
    lv_obj_set_style_border_width(z_line, 0, 0);
    lv_obj_center(z_line);

    // Draggable knob (70x70, purple, centered initially)
    lv_obj_t *z_knob = lv_obj_create(z_bg);
    lv_obj_set_size(z_knob, 70, 70);
    lv_obj_set_style_radius(z_knob, 15, 0);
    lv_obj_set_style_bg_color(z_knob, lv_color_hex(0x9B59B6), 0);
    lv_obj_set_style_border_width(z_knob, 3, 0);
    lv_obj_set_style_border_color(z_knob, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(z_knob);
    
    // Add label to Z knob
    lv_obj_t *z_knob_label = lv_label_create(z_knob);
    lv_label_set_text(z_knob_label, "Z");
    lv_obj_set_style_text_font(z_knob_label, &lv_font_montserrat_20, 0);
    lv_obj_center(z_knob_label);
    
    // Add drag event to knob
    lv_obj_add_flag(z_knob, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(z_knob, z_joystick_event_handler, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(z_knob, z_joystick_event_handler, LV_EVENT_RELEASED, NULL);
}
