#include "ui/tabs/control/ui_tab_control_joystick.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "network/fluidnc_client.h"
#include <lvgl.h>
#include <math.h>

// Static label pointers for real-time updates
static lv_obj_t *xy_percent_label = NULL;
static lv_obj_t *xy_feedrate_label = NULL;
static lv_obj_t *z_percent_label = NULL;
static lv_obj_t *z_feedrate_label = NULL;

// Jogging state tracking
static bool xy_jogging = false;
static bool z_jogging = false;
static float last_xy_x_percent = 0.0f;
static float last_xy_y_percent = 0.0f;
static float last_z_percent = 0.0f;
static unsigned long last_jog_time = 0;

// Label update throttling (only update when values change significantly)
static int last_displayed_xy_percent = -1;
static int last_displayed_xy_feedrate = -1;
static int last_displayed_z_percent = -999;  // Use -999 to ensure first update
static int last_displayed_z_feedrate = -1;

// Jogging parameters
static const unsigned long JOG_INTERVAL_MS = 50;  // Send jog command every 50ms (20Hz for balance of smoothness and performance)
static const float JOG_TIME_INCREMENT = 0.050f;   // 50ms in seconds (nominal dt)

// Apply response curve to joystick input for fine-grained control near center
// Uses quadratic curve: output = sign(input) * (input/100)^2 * 100
// This gives smooth, precise control near center and quick ramp-up at edges
static float applyJoystickCurve(float percent) {
    if (fabs(percent) < 0.1f) return 0.0f;  // Dead zone for stability
    
    // Normalize to 0-1 range, apply quadratic curve, scale back
    float normalized = percent / 100.0f;
    float curved = normalized * normalized;  // Quadratic curve
    
    // Preserve sign and scale back to percentage
    return (percent >= 0.0f ? 1.0f : -1.0f) * curved * 100.0f;
}

// Send jog cancel command (realtime command 0x85)
static void sendJogCancel() {
    // Send as null-terminated string (same format as jog tab uses)
    FluidNCClient::sendCommand("\x85");
    Serial.println("Joystick: Sent jog cancel (0x85)");
}

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
        
        // Calculate radial distance percentage (0 to 100)
        float radial_percent = (distance * 100.0f) / max_radius;
        if (radial_percent > 100.0f) radial_percent = 100.0f;
        
        // Apply response curve to radial distance for fine-grained control near center
        float radial_percent_curved = applyJoystickCurve(radial_percent);
        
        // Calculate direction angle (preserve exact direction)
        float angle = atan2(dy, dx);
        
        // Decompose curved magnitude back to X and Y components
        // This ensures we reach 100% at the edge regardless of angle
        float x_percent_curved = (radial_percent_curved / 100.0f) * cos(angle) * 100.0f;
        float y_percent_curved = -(radial_percent_curved / 100.0f) * sin(angle) * 100.0f;  // Invert Y axis
        
        // Calculate feedrate based on curved radial distance (use max feedrate from settings)
        int max_xy_feed = UITabSettingsJog::getMaxXYFeed();
        int xy_feedrate = (int)(radial_percent_curved * max_xy_feed / 100.0f);
        
        // Update labels ONLY if values changed significantly (reduce LVGL redraws)
        int current_percent = (int)radial_percent_curved;  // Display curved percentage
        if (abs(current_percent - last_displayed_xy_percent) >= 5) {
            if (xy_percent_label != NULL) {
                lv_label_set_text_fmt(xy_percent_label, "XY: %d%%", current_percent);
            }
            last_displayed_xy_percent = current_percent;
        }
        
        if (abs(xy_feedrate - last_displayed_xy_feedrate) >= 50) {
            if (xy_feedrate_label != NULL) {
                lv_label_set_text_fmt(xy_feedrate_label, "%d mm/min", xy_feedrate);
            }
            last_displayed_xy_feedrate = xy_feedrate;
        }
        
        // Send jog command at regular intervals
        unsigned long current_time = millis();
        if (!xy_jogging || (current_time - last_jog_time >= JOG_INTERVAL_MS)) {
            // Calculate actual time delta for accurate positioning
            float actual_dt = xy_jogging ? ((current_time - last_jog_time) / 1000.0f) : JOG_TIME_INCREMENT;
            
            // Calculate feed rate components (mm/min) using curved percentages and max feedrate from settings
            int max_xy_feed = UITabSettingsJog::getMaxXYFeed();
            float v_x = (x_percent_curved / 100.0f) * max_xy_feed;  // mm/min (curved)
            float v_y = (y_percent_curved / 100.0f) * max_xy_feed;  // mm/min (curved)
            float feed_rate = sqrt(v_x * v_x + v_y * v_y);   // Vector magnitude
            
            if (feed_rate > 1.0f) {  // Only send if moving significantly
                // Calculate incremental distances: s = v * dt
                // Convert feed_rate from mm/min to mm/sec, then multiply by actual time increment
                float v_x_mm_per_sec = v_x / 60.0f;
                float v_y_mm_per_sec = v_y / 60.0f;
                
                float x_distance = v_x_mm_per_sec * actual_dt;
                float y_distance = v_y_mm_per_sec * actual_dt;
                
                // Build and send jog command: $J=G91 X[dist] Y[dist] F[rate]
                char jog_cmd[64];
                snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 X%.4f Y%.4f F%.0f\n", 
                         x_distance, y_distance, feed_rate);
                FluidNCClient::sendCommand(jog_cmd);
                
                xy_jogging = true;
                last_jog_time = current_time;
                last_xy_x_percent = x_percent_curved;
                last_xy_y_percent = y_percent_curved;
                
                // Serial debug disabled for performance (uncomment only when debugging)
                // Serial.printf("XY Jog: %s", jog_cmd);
            }
        }
    }
    else if (code == LV_EVENT_RELEASED) {
        // Return knob to center when released
        lv_obj_center(knob);
        
        // Reset labels to 0
        if (xy_percent_label != NULL) {
            lv_label_set_text(xy_percent_label, "XY: 0%");
        }
        if (xy_feedrate_label != NULL) {
            lv_label_set_text(xy_feedrate_label, "0 mm/min");
        }
        
        // Reset last displayed values so next press updates immediately
        last_displayed_xy_percent = -1;
        last_displayed_xy_feedrate = -1;
        
        // Send jog cancel if we were jogging
        if (xy_jogging) {
            sendJogCancel();
            xy_jogging = false;
            last_xy_x_percent = 0.0f;
            last_xy_y_percent = 0.0f;
        }
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
        
        // Apply response curve for fine-grained control near center
        float z_percent_curved = applyJoystickCurve(z_percent);
        
        // Calculate feedrate (absolute value for display) using curved percentage and max feedrate from settings
        int max_z_feed = UITabSettingsJog::getMaxZFeed();
        int z_feedrate = (int)(fabs(z_percent_curved) * max_z_feed / 100.0f);
        
        // Update labels ONLY if values changed significantly (reduce LVGL redraws)
        int current_percent = (int)z_percent_curved;  // Display curved percentage
        if (abs(current_percent - last_displayed_z_percent) >= 5) {
            if (z_percent_label != NULL) {
                lv_label_set_text_fmt(z_percent_label, "Z: %d%%", current_percent);
            }
            last_displayed_z_percent = current_percent;
        }
        
        if (abs(z_feedrate - last_displayed_z_feedrate) >= 50) {
            if (z_feedrate_label != NULL) {
                lv_label_set_text_fmt(z_feedrate_label, "%d mm/min", z_feedrate);
            }
            last_displayed_z_feedrate = z_feedrate;
        }
        
        // Send jog command at regular intervals
        unsigned long current_time = millis();
        if (!z_jogging || (current_time - last_jog_time >= JOG_INTERVAL_MS)) {
            // Calculate actual time delta for accurate positioning
            float actual_dt = z_jogging ? ((current_time - last_jog_time) / 1000.0f) : JOG_TIME_INCREMENT;
            
            // Calculate feed rate with sign (mm/min) using curved percentage and max feedrate from settings
            int max_z_feed = UITabSettingsJog::getMaxZFeed();
            float v_z = (z_percent_curved / 100.0f) * max_z_feed;  // mm/min (with sign, curved)
            float feed_rate = fabs(v_z);  // Absolute value for F parameter
            
            if (feed_rate > 1.0f) {  // Only send if moving significantly
                // Calculate incremental distance: s = v * dt
                // Convert from mm/min to mm/sec, then multiply by actual time increment
                float v_z_mm_per_sec = v_z / 60.0f;
                float z_distance = v_z_mm_per_sec * actual_dt;
                
                // Build and send jog command: $J=G91 Z[dist] F[rate]
                char jog_cmd[64];
                snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 Z%.4f F%.0f\n", 
                         z_distance, feed_rate);
                FluidNCClient::sendCommand(jog_cmd);
                
                z_jogging = true;
                last_jog_time = current_time;
                last_z_percent = z_percent;
                
                // Serial debug disabled for performance (uncomment only when debugging)
                // Serial.printf("Z Jog: %s", jog_cmd);
            }
        }
    }
    else if (code == LV_EVENT_RELEASED) {
        // Return knob to center when released
        lv_obj_center(knob);
        
        // Reset labels to 0
        if (z_percent_label != NULL) {
            lv_label_set_text(z_percent_label, "Z: 0%");
        }
        if (z_feedrate_label != NULL) {
            lv_label_set_text(z_feedrate_label, "0 mm/min");
        }
        
        // Reset last displayed values so next press updates immediately
        last_displayed_z_percent = -999;
        last_displayed_z_feedrate = -1;
        
        // Send jog cancel if we were jogging
        if (z_jogging) {
            sendJogCancel();
            z_jogging = false;
            last_z_percent = 0.0f;
        }
    }
}

void UITabControlJoystick::create(lv_obj_t *parent) {
    // Set parent to use horizontal flex layout (XY joystick on left, info in center, Z slider on right)
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(parent, 15, 0);  // Shift everything right by 5px (10px default + 5px offset)
    lv_obj_set_style_pad_right(parent, 10, 0);
    lv_obj_set_style_pad_top(parent, 10, 0);
    lv_obj_set_style_pad_bottom(parent, 10, 0);
    lv_obj_set_style_pad_gap(parent, 20, 0);

    // ========== XY Joystick (Circular) ==========
    lv_obj_t *xy_container = lv_obj_create(parent);
    lv_obj_set_size(xy_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(xy_container, 5, 0);
    lv_obj_set_flex_flow(xy_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(xy_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(xy_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(xy_container, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(xy_container, 0, 0);  // No border
    
    // XY Label (centered above joystick) - Settings-style header with XY axis color
    lv_obj_t *xy_label = lv_label_create(xy_container);
    lv_label_set_text(xy_label, "XY JOG");
    lv_obj_set_style_text_font(xy_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(xy_label, UITheme::AXIS_XY, 0);

    // Background circle (220x220 with crosshairs)
    lv_obj_t *xy_bg = lv_obj_create(xy_container);
    lv_obj_set_size(xy_bg, 220, 220);
    lv_obj_set_style_radius(xy_bg, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(xy_bg, UITheme::JOYSTICK_BG, 0);
    lv_obj_set_style_border_width(xy_bg, 2, 0);
    lv_obj_set_style_border_color(xy_bg, UITheme::JOYSTICK_BORDER, 0);
    lv_obj_clear_flag(xy_bg, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

    // Horizontal crosshair (extends to circle edge)
    lv_obj_t *xy_h_line = lv_obj_create(xy_bg);
    lv_obj_set_size(xy_h_line, 200, 2);  // 200px width
    lv_obj_set_style_bg_color(xy_h_line, UITheme::JOYSTICK_LINE, 0);
    lv_obj_set_style_border_width(xy_h_line, 0, 0);
    lv_obj_center(xy_h_line);

    // Vertical crosshair (extends to circle edge)
    lv_obj_t *xy_v_line = lv_obj_create(xy_bg);
    lv_obj_set_size(xy_v_line, 2, 200);  // 200px height
    lv_obj_set_style_bg_color(xy_v_line, UITheme::JOYSTICK_LINE, 0);
    lv_obj_set_style_border_width(xy_v_line, 0, 0);
    lv_obj_center(xy_v_line);

    // Draggable knob (90x90, cyan, centered initially)
    lv_obj_t *xy_knob = lv_obj_create(xy_bg);
    lv_obj_set_size(xy_knob, 90, 90);
    lv_obj_set_style_radius(xy_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(xy_knob, UITheme::JOYSTICK_XY, 0);
    lv_obj_set_style_border_width(xy_knob, 3, 0);
    lv_obj_set_style_border_color(xy_knob, lv_color_white(), 0);
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

    // ========== Center Info Display (XY + Z values) ==========
    lv_obj_t *info_container = lv_obj_create(parent);
    lv_obj_set_size(info_container, 160, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(info_container, 10, 0);
    lv_obj_set_flex_flow(info_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(info_container, 10, 0);
    lv_obj_clear_flag(info_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(info_container, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(info_container, 0, 0);  // No border
    
    // XY Percentage (radial distance)
    xy_percent_label = lv_label_create(info_container);
    lv_label_set_text(xy_percent_label, "XY: 0%");
    lv_obj_set_style_text_font(xy_percent_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(xy_percent_label, UITheme::JOYSTICK_XY, 0);
    
    // XY Feedrate
    xy_feedrate_label = lv_label_create(info_container);
    lv_label_set_text(xy_feedrate_label, "0 mm/min");
    lv_obj_set_style_text_font(xy_feedrate_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(xy_feedrate_label, UITheme::TEXT_LIGHT, 0);
    
    // XY Max Feedrate (from settings)
    lv_obj_t *xy_max_label = lv_label_create(info_container);
    char xy_max_text[32];
    snprintf(xy_max_text, sizeof(xy_max_text), "Max: %d mm/min", UITabSettingsJog::getMaxXYFeed());
    lv_label_set_text(xy_max_label, xy_max_text);
    lv_obj_set_style_text_font(xy_max_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(xy_max_label, UITheme::UI_INFO, 0);
    
    // Spacer between XY and Z info
    lv_obj_t *spacer = lv_obj_create(info_container);
    lv_obj_set_size(spacer, 1, 20);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    
    // Z Percentage
    z_percent_label = lv_label_create(info_container);
    lv_label_set_text(z_percent_label, "Z: 0%");
    lv_obj_set_style_text_font(z_percent_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(z_percent_label, UITheme::AXIS_Z, 0);
    
    // Z Feedrate
    z_feedrate_label = lv_label_create(info_container);
    lv_label_set_text(z_feedrate_label, "0 mm/min");
    lv_obj_set_style_text_font(z_feedrate_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(z_feedrate_label, UITheme::TEXT_LIGHT, 0);
    
    // Z Max Feedrate (from settings)
    lv_obj_t *z_max_label = lv_label_create(info_container);
    char z_max_text[32];
    snprintf(z_max_text, sizeof(z_max_text), "Max: %d mm/min", UITabSettingsJog::getMaxZFeed());
    lv_label_set_text(z_max_label, z_max_text);
    lv_obj_set_style_text_font(z_max_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_max_label, UITheme::UI_INFO, 0);

    // ========== Z Slider (Vertical) ==========
    lv_obj_t *z_container = lv_obj_create(parent);
    lv_obj_set_size(z_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(z_container, 5, 0);
    lv_obj_set_flex_flow(z_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(z_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(z_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(z_container, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(z_container, 0, 0);  // No border
    
    // Z Label (centered above slider) - Settings-style header with Z axis color
    lv_obj_t *z_label = lv_label_create(z_container);
    lv_label_set_text(z_label, "Z JOG");
    lv_obj_set_style_text_font(z_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(z_label, UITheme::AXIS_Z, 0);

    // Background slider (80x220 vertical)
    lv_obj_t *z_bg = lv_obj_create(z_container);
    lv_obj_set_size(z_bg, 80, 220);
    lv_obj_set_style_radius(z_bg, 15, 0);
    lv_obj_set_style_bg_color(z_bg, UITheme::JOYSTICK_BG, 0);
    lv_obj_set_style_border_width(z_bg, 2, 0);
    lv_obj_set_style_border_color(z_bg, UITheme::JOYSTICK_BORDER, 0);
    lv_obj_clear_flag(z_bg, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

    // Center line
    lv_obj_t *z_line = lv_obj_create(z_bg);
    lv_obj_set_size(z_line, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(z_line, UITheme::JOYSTICK_LINE, 0);
    lv_obj_set_style_border_width(z_line, 0, 0);
    lv_obj_center(z_line);

    // Draggable knob (70x70, purple, centered initially)
    lv_obj_t *z_knob = lv_obj_create(z_bg);
    lv_obj_set_size(z_knob, 70, 70);
    lv_obj_set_style_radius(z_knob, 15, 0);
    lv_obj_set_style_bg_color(z_knob, UITheme::JOYSTICK_Z, 0);
    lv_obj_set_style_border_width(z_knob, 3, 0);
    lv_obj_set_style_border_color(z_knob, lv_color_white(), 0);
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
