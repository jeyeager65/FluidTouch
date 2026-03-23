#include "ui/tabs/control/ui_tab_control_joystick.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "network/fluidnc_client.h"
#include <lvgl.h>
#include <math.h>

// Axis mode selection
enum AxisMode {
    MODE_XY = 0,
    MODE_X = 1,
    MODE_Y = 2
};

static AxisMode current_axis_mode = MODE_XY;
static lv_obj_t *xy_joystick_container = NULL;
static lv_obj_t *xy_bg = NULL;
static lv_obj_t *xy_knob = NULL;

// Axis selection button references
static lv_obj_t *btn_xy = NULL;
static lv_obj_t *btn_x = NULL;
static lv_obj_t *btn_y = NULL;

// XY jog label (updates based on mode)
static lv_obj_t *xy_jog_label = NULL;

// Z/A Toggle (only created if A-axis enabled)
static lv_obj_t *btn_z_mode = NULL;
static lv_obj_t *btn_a_mode = NULL;
static lv_obj_t *za_jog_label = NULL;  // "Z JOG" or "A JOG"
static bool is_z_mode = true;  // true = Z mode, false = A mode

// Z slider references
static lv_obj_t *z_container = NULL;
static lv_obj_t *z_bg = NULL;
static lv_obj_t *z_knob = NULL;
static lv_obj_t *z_line = NULL;

// A-axis slider references (same positions as Z)
static lv_obj_t *a_container = NULL;
static lv_obj_t *a_bg = NULL;
static lv_obj_t *a_knob = NULL;
static lv_obj_t *a_line = NULL;

// Static label pointers for real-time updates
static lv_obj_t *xy_percent_label = NULL;
static lv_obj_t *xy_feedrate_label = NULL;
static lv_obj_t *z_percent_label = NULL;
static lv_obj_t *z_feedrate_label = NULL;
static lv_obj_t *z_max_label = NULL;
static lv_obj_t *a_percent_label = NULL;
static lv_obj_t *a_feedrate_label = NULL;
static lv_obj_t *a_max_label = NULL;

// Jogging state tracking
static bool xy_jogging = false;
static bool z_jogging = false;
static bool a_jogging = false;
static float last_xy_x_percent = 0.0f;
static float last_xy_y_percent = 0.0f;
static float last_z_percent = 0.0f;
static float last_a_percent = 0.0f;
static unsigned long last_jog_time = 0;

// Label update throttling (only update when values change significantly)
static int last_displayed_xy_percent = -1;
static int last_displayed_xy_feedrate = -1;
static int last_displayed_z_percent = -999;  // Use -999 to ensure first update
static int last_displayed_z_feedrate = -1;
static int last_displayed_a_percent = -999;
static int last_displayed_a_feedrate = -1;

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

// X-only Joystick drag event handler (horizontal slider)
static void x_joystick_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
    
    if (!knob || !lv_obj_is_valid(knob)) return;
    
    if (code == LV_EVENT_PRESSING) {
        lv_obj_t *bg = lv_obj_get_parent(knob);
        if (!bg || !lv_obj_is_valid(bg)) return;
        
        lv_indev_t *indev = lv_indev_get_act();
        if (indev == NULL) return;
        
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        int32_t bg_w = lv_obj_get_width(bg);
        lv_area_t bg_coords;
        lv_obj_get_coords(bg, &bg_coords);
        int32_t bg_center_x = (bg_coords.x1 + bg_coords.x2) / 2;
        int32_t dx = point.x - bg_center_x;
        int32_t max_offset = (bg_w / 2) - (lv_obj_get_width(knob) / 2) - 5;
        
        if (dx < -max_offset) dx = -max_offset;
        if (dx > max_offset) dx = max_offset;
        
        lv_obj_align(knob, LV_ALIGN_CENTER, dx, 0);
        
        float x_percent = (dx * 100.0f) / max_offset;
        float x_percent_curved = applyJoystickCurve(x_percent);
        
        int max_xy_feed = UITabSettingsJog::getMaxXYFeed();
        int x_feedrate = (int)(fabs(x_percent_curved) * max_xy_feed / 100.0f);
        
        int current_percent = (int)x_percent_curved;
        if (abs(current_percent - last_displayed_xy_percent) >= 5) {
            if (xy_percent_label != NULL) {
                lv_label_set_text_fmt(xy_percent_label, "X: %d%%", current_percent);
            }
            last_displayed_xy_percent = current_percent;
        }
        
        if (abs(x_feedrate - last_displayed_xy_feedrate) >= 50) {
            if (xy_feedrate_label != NULL) {
                lv_label_set_text_fmt(xy_feedrate_label, "%d mm/min", x_feedrate);
            }
            last_displayed_xy_feedrate = x_feedrate;
        }
        
        unsigned long current_time = millis();
        if (!xy_jogging || (current_time - last_jog_time >= JOG_INTERVAL_MS)) {
            float actual_dt = xy_jogging ? ((current_time - last_jog_time) / 1000.0f) : JOG_TIME_INCREMENT;
            float v_x = (x_percent_curved / 100.0f) * max_xy_feed;
            float feed_rate = fabs(v_x);
            
            if (feed_rate > 1.0f) {
                float v_x_mm_per_sec = v_x / 60.0f;
                float x_distance = v_x_mm_per_sec * actual_dt;
                
                char jog_cmd[64];
                snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 X%.4f F%.0f\n", x_distance, feed_rate);
                FluidNCClient::sendCommand(jog_cmd);
                
                xy_jogging = true;
                last_jog_time = current_time;
                last_xy_x_percent = x_percent_curved;
            }
        }
    }
    else if (code == LV_EVENT_RELEASED) {
        lv_obj_center(knob);
        if (xy_percent_label != NULL) lv_label_set_text(xy_percent_label, "X: 0%");
        if (xy_feedrate_label != NULL) lv_label_set_text(xy_feedrate_label, "0 mm/min");
        last_displayed_xy_percent = -1;
        last_displayed_xy_feedrate = -1;
        if (xy_jogging) {
            sendJogCancel();
            xy_jogging = false;
            last_xy_x_percent = 0.0f;
        }
    }
}

// Y-only Joystick drag event handler (vertical slider)
static void y_joystick_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *knob = (lv_obj_t *)lv_event_get_target(e);
    
    if (!knob || !lv_obj_is_valid(knob)) return;
    
    if (code == LV_EVENT_PRESSING) {
        lv_obj_t *bg = lv_obj_get_parent(knob);
        if (!bg || !lv_obj_is_valid(bg)) return;
        
        lv_indev_t *indev = lv_indev_get_act();
        if (indev == NULL) return;
        
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        
        int32_t bg_h = lv_obj_get_height(bg);
        lv_area_t bg_coords;
        lv_obj_get_coords(bg, &bg_coords);
        int32_t bg_center_y = (bg_coords.y1 + bg_coords.y2) / 2;
        int32_t dy = point.y - bg_center_y;
        int32_t max_offset = (bg_h / 2) - (lv_obj_get_height(knob) / 2) - 5;
        
        if (dy < -max_offset) dy = -max_offset;
        if (dy > max_offset) dy = max_offset;
        
        lv_obj_align(knob, LV_ALIGN_CENTER, 0, dy);
        
        float y_percent = -(dy * 100.0f) / max_offset;  // Invert Y
        float y_percent_curved = applyJoystickCurve(y_percent);
        
        int max_xy_feed = UITabSettingsJog::getMaxXYFeed();
        int y_feedrate = (int)(fabs(y_percent_curved) * max_xy_feed / 100.0f);
        
        int current_percent = (int)y_percent_curved;
        if (abs(current_percent - last_displayed_xy_percent) >= 5) {
            if (xy_percent_label != NULL) {
                lv_label_set_text_fmt(xy_percent_label, "Y: %d%%", current_percent);
            }
            last_displayed_xy_percent = current_percent;
        }
        
        if (abs(y_feedrate - last_displayed_xy_feedrate) >= 50) {
            if (xy_feedrate_label != NULL) {
                lv_label_set_text_fmt(xy_feedrate_label, "%d mm/min", y_feedrate);
            }
            last_displayed_xy_feedrate = y_feedrate;
        }
        
        unsigned long current_time = millis();
        if (!xy_jogging || (current_time - last_jog_time >= JOG_INTERVAL_MS)) {
            float actual_dt = xy_jogging ? ((current_time - last_jog_time) / 1000.0f) : JOG_TIME_INCREMENT;
            float v_y = (y_percent_curved / 100.0f) * max_xy_feed;
            float feed_rate = fabs(v_y);
            
            if (feed_rate > 1.0f) {
                float v_y_mm_per_sec = v_y / 60.0f;
                float y_distance = v_y_mm_per_sec * actual_dt;
                
                char jog_cmd[64];
                snprintf(jog_cmd, sizeof(jog_cmd), "$J=G91 Y%.4f F%.0f\n", y_distance, feed_rate);
                FluidNCClient::sendCommand(jog_cmd);
                
                xy_jogging = true;
                last_jog_time = current_time;
                last_xy_y_percent = y_percent_curved;
            }
        }
    }
    else if (code == LV_EVENT_RELEASED) {
        lv_obj_center(knob);
        if (xy_percent_label != NULL) lv_label_set_text(xy_percent_label, "Y: 0%");
        if (xy_feedrate_label != NULL) lv_label_set_text(xy_feedrate_label, "0 mm/min");
        last_displayed_xy_percent = -1;
        last_displayed_xy_feedrate = -1;
        if (xy_jogging) {
            sendJogCancel();
            xy_jogging = false;
            last_xy_y_percent = 0.0f;
        }
    }
}

// Rebuild the joystick display based on current axis mode
static void rebuildJoystick() {
    if (!xy_joystick_container || !lv_obj_is_valid(xy_joystick_container)) return;
    
    // Delete old background and knob if they exist
    if (xy_bg && lv_obj_is_valid(xy_bg)) {
        lv_obj_del(xy_bg);
        xy_bg = NULL;
        xy_knob = NULL;  // Knob is child of bg, deleted automatically
    }
    
    if (current_axis_mode == MODE_XY) {
        // Create circular XY joystick (220x220, centered)
        xy_bg = lv_obj_create(xy_joystick_container);
        lv_obj_set_size(xy_bg, 220, 220);
        lv_obj_set_style_radius(xy_bg, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(xy_bg, UITheme::JOYSTICK_BG, 0);
        lv_obj_set_style_border_width(xy_bg, 2, 0);
        lv_obj_set_style_border_color(xy_bg, UITheme::JOYSTICK_BORDER, 0);
        lv_obj_clear_flag(xy_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(xy_bg);  // Center in container
        
        // Horizontal crosshair
        lv_obj_t *xy_h_line = lv_obj_create(xy_bg);
        lv_obj_set_size(xy_h_line, 200, 2);
        lv_obj_set_style_bg_color(xy_h_line, UITheme::JOYSTICK_LINE, 0);
        lv_obj_set_style_border_width(xy_h_line, 0, 0);
        lv_obj_center(xy_h_line);
        
        // Vertical crosshair
        lv_obj_t *xy_v_line = lv_obj_create(xy_bg);
        lv_obj_set_size(xy_v_line, 2, 200);
        lv_obj_set_style_bg_color(xy_v_line, UITheme::JOYSTICK_LINE, 0);
        lv_obj_set_style_border_width(xy_v_line, 0, 0);
        lv_obj_center(xy_v_line);
        
        // Draggable knob
        xy_knob = lv_obj_create(xy_bg);
        lv_obj_set_size(xy_knob, 90, 90);
        lv_obj_set_style_radius(xy_knob, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(xy_knob, UITheme::JOYSTICK_XY, 0);
        lv_obj_set_style_border_width(xy_knob, 3, 0);
        lv_obj_set_style_border_color(xy_knob, lv_color_white(), 0);
        lv_obj_center(xy_knob);
        
        lv_obj_t *xy_knob_label = lv_label_create(xy_knob);
        lv_label_set_text(xy_knob_label, "XY");
        lv_obj_set_style_text_font(xy_knob_label, &lv_font_montserrat_20, 0);
        lv_obj_center(xy_knob_label);
        
        lv_obj_add_flag(xy_knob, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(xy_knob, xy_joystick_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(xy_knob, xy_joystick_event_handler, LV_EVENT_RELEASED, NULL);
    }
    else if (current_axis_mode == MODE_X) {
        // Create horizontal X slider (220x80, centered vertically to align knob with Z knob)
        xy_bg = lv_obj_create(xy_joystick_container);
        lv_obj_set_size(xy_bg, 220, 80);
        lv_obj_set_style_radius(xy_bg, 15, 0);
        lv_obj_set_style_bg_color(xy_bg, UITheme::JOYSTICK_BG, 0);
        lv_obj_set_style_border_width(xy_bg, 2, 0);
        lv_obj_set_style_border_color(xy_bg, UITheme::JOYSTICK_BORDER, 0);
        lv_obj_clear_flag(xy_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(xy_bg);  // Center in container - knob will be at same position as XY center
        
        // Center line
        lv_obj_t *x_line = lv_obj_create(xy_bg);
        lv_obj_set_size(x_line, 2, LV_PCT(100));
        lv_obj_set_style_bg_color(x_line, UITheme::JOYSTICK_LINE, 0);
        lv_obj_set_style_border_width(x_line, 0, 0);
        lv_obj_center(x_line);
        
        // Draggable knob
        xy_knob = lv_obj_create(xy_bg);
        lv_obj_set_size(xy_knob, 70, 70);
        lv_obj_set_style_radius(xy_knob, 15, 0);
        lv_obj_set_style_bg_color(xy_knob, UITheme::AXIS_X, 0);
        lv_obj_set_style_border_width(xy_knob, 3, 0);
        lv_obj_set_style_border_color(xy_knob, lv_color_white(), 0);
        lv_obj_center(xy_knob);
        
        lv_obj_t *x_knob_label = lv_label_create(xy_knob);
        lv_label_set_text(x_knob_label, "X");
        lv_obj_set_style_text_font(x_knob_label, &lv_font_montserrat_20, 0);
        lv_obj_center(x_knob_label);
        
        lv_obj_add_flag(xy_knob, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(xy_knob, x_joystick_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(xy_knob, x_joystick_event_handler, LV_EVENT_RELEASED, NULL);
    }
    else if (current_axis_mode == MODE_Y) {
        // Create vertical Y slider (80x220, centered horizontally to align knob with XY center)
        xy_bg = lv_obj_create(xy_joystick_container);
        lv_obj_set_size(xy_bg, 80, 220);
        lv_obj_set_style_radius(xy_bg, 15, 0);
        lv_obj_set_style_bg_color(xy_bg, UITheme::JOYSTICK_BG, 0);
        lv_obj_set_style_border_width(xy_bg, 2, 0);
        lv_obj_set_style_border_color(xy_bg, UITheme::JOYSTICK_BORDER, 0);
        lv_obj_clear_flag(xy_bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_center(xy_bg);  // Center in container - knob will be at same position as XY center
        
        // Center line
        lv_obj_t *y_line = lv_obj_create(xy_bg);
        lv_obj_set_size(y_line, LV_PCT(100), 2);
        lv_obj_set_style_bg_color(y_line, UITheme::JOYSTICK_LINE, 0);
        lv_obj_set_style_border_width(y_line, 0, 0);
        lv_obj_center(y_line);
        
        // Draggable knob
        xy_knob = lv_obj_create(xy_bg);
        lv_obj_set_size(xy_knob, 70, 70);
        lv_obj_set_style_radius(xy_knob, 15, 0);
        lv_obj_set_style_bg_color(xy_knob, UITheme::AXIS_Y, 0);
        lv_obj_set_style_border_width(xy_knob, 3, 0);
        lv_obj_set_style_border_color(xy_knob, lv_color_white(), 0);
        lv_obj_center(xy_knob);
        
        lv_obj_t *y_knob_label = lv_label_create(xy_knob);
        lv_label_set_text(y_knob_label, "Y");
        lv_obj_set_style_text_font(y_knob_label, &lv_font_montserrat_20, 0);
        lv_obj_center(y_knob_label);
        
        lv_obj_add_flag(xy_knob, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(xy_knob, y_joystick_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(xy_knob, y_joystick_event_handler, LV_EVENT_RELEASED, NULL);
    }
}

// Axis selection button event handler
static void axis_button_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        AxisMode new_mode = (AxisMode)(intptr_t)lv_event_get_user_data(e);
        
        if (new_mode != current_axis_mode) {
            // Cancel any active jogging before switching
            if (xy_jogging) {
                sendJogCancel();
                xy_jogging = false;
            }
            
            current_axis_mode = new_mode;
            
            // Update button borders to show selection
            if (btn_xy && lv_obj_is_valid(btn_xy)) {
                if (new_mode == MODE_XY) {
                    lv_obj_set_style_border_width(btn_xy, 3, LV_PART_MAIN);
                    lv_obj_set_style_border_color(btn_xy, lv_color_white(), LV_PART_MAIN);
                } else {
                    lv_obj_set_style_border_width(btn_xy, 0, LV_PART_MAIN);
                }
            }
            
            if (btn_x && lv_obj_is_valid(btn_x)) {
                if (new_mode == MODE_X) {
                    lv_obj_set_style_border_width(btn_x, 3, LV_PART_MAIN);
                    lv_obj_set_style_border_color(btn_x, lv_color_white(), LV_PART_MAIN);
                } else {
                    lv_obj_set_style_border_width(btn_x, 0, LV_PART_MAIN);
                }
            }
            
            if (btn_y && lv_obj_is_valid(btn_y)) {
                if (new_mode == MODE_Y) {
                    lv_obj_set_style_border_width(btn_y, 3, LV_PART_MAIN);
                    lv_obj_set_style_border_color(btn_y, lv_color_white(), LV_PART_MAIN);
                } else {
                    lv_obj_set_style_border_width(btn_y, 0, LV_PART_MAIN);
                }
            }
            
            rebuildJoystick();
            
            // Update XY JOG label text and color based on mode
            if (xy_jog_label != NULL && lv_obj_is_valid(xy_jog_label)) {
                const char *jog_label_text = (new_mode == MODE_XY) ? "XY JOG" : 
                                             (new_mode == MODE_X) ? "X JOG" : "Y JOG";
                lv_label_set_text(xy_jog_label, jog_label_text);
                
                // Update color to match axis
                lv_color_t jog_label_color = (new_mode == MODE_XY) ? UITheme::AXIS_XY : 
                                             (new_mode == MODE_X) ? UITheme::AXIS_X : UITheme::AXIS_Y;
                lv_obj_set_style_text_color(xy_jog_label, jog_label_color, 0);
            }
            
            // Reset info labels and update colors based on mode
            if (xy_percent_label != NULL) {
                const char *label = (new_mode == MODE_XY) ? "XY: 0%" : 
                                    (new_mode == MODE_X) ? "X: 0%" : "Y: 0%";
                lv_label_set_text(xy_percent_label, label);
                
                // Update color to match axis
                lv_color_t color = (new_mode == MODE_XY) ? UITheme::JOYSTICK_XY : 
                                   (new_mode == MODE_X) ? UITheme::AXIS_X : UITheme::AXIS_Y;
                lv_obj_set_style_text_color(xy_percent_label, color, 0);
            }
            if (xy_feedrate_label != NULL) {
                lv_label_set_text(xy_feedrate_label, "0 mm/min");
            }
            last_displayed_xy_percent = -1;
            last_displayed_xy_feedrate = -1;
        }
    }
}

// A-axis joystick drag event handler (vertical slider)
static void a_joystick_event_handler(lv_event_t *e) {
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

        int32_t bg_center_y = bg_coords.y1 + bg_h / 2;

        // Calculate vertical offset from center (positive = down, negative = up)
        int32_t dy = point.y - bg_center_y;

        // Constrain knob movement within slider bounds
        int32_t knob_size = lv_obj_get_height(knob);
        int32_t max_offset = (bg_h - knob_size) / 2;
        if (dy > max_offset) dy = max_offset;
        if (dy < -max_offset) dy = -max_offset;

        // Position knob (centered horizontally, offset vertically)
        lv_obj_align(knob, LV_ALIGN_CENTER, 0, dy);

        // Calculate A percentage (inverted: up = positive)
        float a_percent = -(dy * 100.0f) / max_offset;

        // Apply response curve for fine control near center
        float a_percent_curved = applyJoystickCurve(a_percent);

        // Get A-axis max feedrate from settings
        int max_feedrate = UITabSettingsJog::getMaxAFeed();

        // Calculate feedrate (proportional to curved percentage)
        int feedrate = (int)(fabs(a_percent_curved) * max_feedrate / 100.0f);
        if (feedrate < 1) feedrate = 1;  // Minimum 1 mm/min when moving

        // Update labels only when value changes significantly (reduce flicker)
        int current_percent_display = (int)a_percent_curved;
        if (abs(current_percent_display - last_displayed_a_percent) >= 5 || !a_jogging) {
            if (a_percent_label != NULL) {
                char percent_text[16];
                snprintf(percent_text, sizeof(percent_text), "A: %d%%", current_percent_display);
                lv_label_set_text(a_percent_label, percent_text);
            }
            last_displayed_a_percent = current_percent_display;
        }

        if (labs(feedrate - last_displayed_a_feedrate) >= 50 || !a_jogging) {
            if (a_feedrate_label != NULL) {
                char feedrate_text[16];
                snprintf(feedrate_text, sizeof(feedrate_text), "%d mm/min", feedrate);
                lv_label_set_text(a_feedrate_label, feedrate_text);
            }
            last_displayed_a_feedrate = feedrate;
        }

        // Send jog commands at intervals (throttle to 20Hz)
        unsigned long current_time = millis();
        if (fabs(a_percent_curved) > 0.1f && (current_time - last_jog_time >= JOG_INTERVAL_MS)) {
            if (FluidNCClient::isConnected()) {
                // Calculate distance: feedrate * time (mm/min * min = mm)
                float distance = (a_percent_curved / 100.0f) * feedrate * (JOG_TIME_INCREMENT / 60.0f);

                // Build jog command: $J=G91 A[distance] F[feedrate]
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "$J=G91 A%.4f F%d\n", distance, feedrate);
                FluidNCClient::sendCommand(cmd);

                a_jogging = true;
                last_a_percent = a_percent_curved;
                last_jog_time = current_time;
            }
        }
    } else if (code == LV_EVENT_RELEASED) {
        // Return knob to center
        lv_obj_center(knob);

        // Reset labels
        if (a_percent_label != NULL) {
            lv_label_set_text(a_percent_label, "A: 0%");
        }
        if (a_feedrate_label != NULL) {
            lv_label_set_text(a_feedrate_label, "0 mm/min");
        }

        // Send jog cancel if we were jogging
        if (a_jogging) {
            sendJogCancel();
            a_jogging = false;
            last_a_percent = 0.0f;

            // Reset throttle state
            last_displayed_a_percent = -999;
            last_displayed_a_feedrate = -1;
        }
    }
}

// Z/A Toggle event handler for joystick
static void za_joystick_toggle_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    // Get which button was clicked: 1 = Z mode, 0 = A mode
    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    bool new_is_z_mode = (mode == 1);

    if (new_is_z_mode == is_z_mode) {
        return;  // Already in this mode
    }

    is_z_mode = new_is_z_mode;

    // Update button borders to show selection
    if (btn_z_mode != NULL && btn_a_mode != NULL) {
        if (is_z_mode) {
            lv_obj_set_style_border_width(btn_z_mode, 3, LV_PART_MAIN);
            lv_obj_set_style_border_color(btn_z_mode, lv_color_white(), LV_PART_MAIN);
            lv_obj_set_style_border_width(btn_a_mode, 0, LV_PART_MAIN);
        } else {
            lv_obj_set_style_border_width(btn_z_mode, 0, LV_PART_MAIN);
            lv_obj_set_style_border_width(btn_a_mode, 3, LV_PART_MAIN);
            lv_obj_set_style_border_color(btn_a_mode, lv_color_white(), LV_PART_MAIN);
        }
    }

    if (is_z_mode) {
        // Switch to Z mode: Show Z, hide A
        lv_label_set_text(za_jog_label, "Z JOG");
        lv_obj_set_style_text_color(za_jog_label, UITheme::AXIS_Z, 0);

        if (z_container != NULL) lv_obj_clear_flag(z_container, LV_OBJ_FLAG_HIDDEN);
        if (a_container != NULL) lv_obj_add_flag(a_container, LV_OBJ_FLAG_HIDDEN);

        // Show Z info labels, hide A info labels
        if (z_percent_label != NULL) lv_obj_clear_flag(z_percent_label, LV_OBJ_FLAG_HIDDEN);
        if (z_feedrate_label != NULL) lv_obj_clear_flag(z_feedrate_label, LV_OBJ_FLAG_HIDDEN);
        if (z_max_label != NULL) lv_obj_clear_flag(z_max_label, LV_OBJ_FLAG_HIDDEN);
        if (a_percent_label != NULL) lv_obj_add_flag(a_percent_label, LV_OBJ_FLAG_HIDDEN);
        if (a_feedrate_label != NULL) lv_obj_add_flag(a_feedrate_label, LV_OBJ_FLAG_HIDDEN);
        if (a_max_label != NULL) lv_obj_add_flag(a_max_label, LV_OBJ_FLAG_HIDDEN);

        // Reset A-axis knob position
        if (a_knob != NULL) lv_obj_center(a_knob);
    } else {
        // Switch to A mode: Show A, hide Z
        lv_label_set_text(za_jog_label, "A JOG");
        lv_obj_set_style_text_color(za_jog_label, UITheme::AXIS_A, 0);

        if (z_container != NULL) lv_obj_add_flag(z_container, LV_OBJ_FLAG_HIDDEN);
        if (a_container != NULL) lv_obj_clear_flag(a_container, LV_OBJ_FLAG_HIDDEN);

        // Hide Z info labels, show A info labels
        if (z_percent_label != NULL) lv_obj_add_flag(z_percent_label, LV_OBJ_FLAG_HIDDEN);
        if (z_feedrate_label != NULL) lv_obj_add_flag(z_feedrate_label, LV_OBJ_FLAG_HIDDEN);
        if (z_max_label != NULL) lv_obj_add_flag(z_max_label, LV_OBJ_FLAG_HIDDEN);
        if (a_percent_label != NULL) lv_obj_clear_flag(a_percent_label, LV_OBJ_FLAG_HIDDEN);
        if (a_feedrate_label != NULL) lv_obj_clear_flag(a_feedrate_label, LV_OBJ_FLAG_HIDDEN);
        if (a_max_label != NULL) lv_obj_clear_flag(a_max_label, LV_OBJ_FLAG_HIDDEN);

        // Reset Z-axis knob position
        if (z_knob != NULL) lv_obj_center(z_knob);
    }

    Serial.printf("[Joystick] Switched to %s mode\n", is_z_mode ? "Z" : "A");
}

void UITabControlJoystick::create(lv_obj_t *parent) {
    // Set parent to use horizontal flex layout (XY joystick on left, info in center, Z slider on right)
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);  // Align to top
    lv_obj_set_style_pad_left(parent, 15, 0);  // Shift everything right by 5px (10px default + 5px offset)
    lv_obj_set_style_pad_right(parent, 10, 0);
    lv_obj_set_style_pad_top(parent, 20, 0);  // Move everything down 10px (was 10px, now 20px)
    lv_obj_set_style_pad_bottom(parent, 10, 0);
    lv_obj_set_style_pad_gap(parent, 20, 0);

    // ========== XY Joystick (Circular) ==========
    lv_obj_t *xy_outer_container = lv_obj_create(parent);
    lv_obj_set_size(xy_outer_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(xy_outer_container, 5, 0);
    lv_obj_set_flex_flow(xy_outer_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(xy_outer_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(xy_outer_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(xy_outer_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(xy_outer_container, 0, 0);
    
    // XY Label (centered above joystick) - will update based on mode
    xy_jog_label = lv_label_create(xy_outer_container);
    lv_label_set_text(xy_jog_label, "XY JOG");
    lv_obj_set_style_text_font(xy_jog_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(xy_jog_label, UITheme::AXIS_XY, 0);

    // Joystick container (will be rebuilt based on mode)
    xy_joystick_container = lv_obj_create(xy_outer_container);
    lv_obj_set_size(xy_joystick_container, 220, 220);
    lv_obj_set_style_pad_all(xy_joystick_container, 0, 0);
    lv_obj_clear_flag(xy_joystick_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(xy_joystick_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(xy_joystick_container, 0, 0);
    
    // Build initial joystick (XY mode)
    rebuildJoystick();
    
    // Axis selection buttons (XY, X, Y)
    lv_obj_t *axis_button_container = lv_obj_create(xy_outer_container);
    lv_obj_set_size(axis_button_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(axis_button_container, 5, 0);
    lv_obj_set_flex_flow(axis_button_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(axis_button_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(axis_button_container, 5, 0);
    lv_obj_clear_flag(axis_button_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(axis_button_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(axis_button_container, 0, 0);
    
    // XY button (initially selected with white border)
    btn_xy = lv_button_create(axis_button_container);
    lv_obj_set_size(btn_xy, 60, 40);
    lv_obj_set_style_bg_color(btn_xy, UITheme::JOYSTICK_XY, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn_xy, 3, LV_PART_MAIN);  // Initial selection border
    lv_obj_set_style_border_color(btn_xy, lv_color_white(), LV_PART_MAIN);
    lv_obj_t *lbl_xy = lv_label_create(btn_xy);
    lv_label_set_text(lbl_xy, "XY");
    lv_obj_set_style_text_font(lbl_xy, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_xy);
    lv_obj_add_event_cb(btn_xy, axis_button_event_handler, LV_EVENT_CLICKED, (void*)MODE_XY);
    
    // X button
    btn_x = lv_button_create(axis_button_container);
    lv_obj_set_size(btn_x, 60, 40);
    lv_obj_set_style_bg_color(btn_x, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_x = lv_label_create(btn_x);
    lv_label_set_text(lbl_x, "X");
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_x);
    lv_obj_add_event_cb(btn_x, axis_button_event_handler, LV_EVENT_CLICKED, (void*)MODE_X);
    
    // Y button
    btn_y = lv_button_create(axis_button_container);
    lv_obj_set_size(btn_y, 60, 40);
    lv_obj_set_style_bg_color(btn_y, UITheme::AXIS_Y, LV_PART_MAIN);
    lv_obj_t *lbl_y = lv_label_create(btn_y);
    lv_label_set_text(lbl_y, "Y");
    lv_obj_set_style_text_font(lbl_y, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl_y);
    lv_obj_add_event_cb(btn_y, axis_button_event_handler, LV_EVENT_CLICKED, (void*)MODE_Y);

    // ========== Center Info Display (XY + Z values) ==========
    lv_obj_t *info_container = lv_obj_create(parent);
    lv_obj_set_size(info_container, 160, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(info_container, 10, 0);
    lv_obj_set_style_pad_top(info_container, 50, 0);  // Additional 30px down (20px from before + 30px = 50px total)
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
    z_max_label = lv_label_create(info_container);
    char z_max_text[32];
    snprintf(z_max_text, sizeof(z_max_text), "Max: %d mm/min", UITabSettingsJog::getMaxZFeed());
    lv_label_set_text(z_max_label, z_max_text);
    lv_obj_set_style_text_font(z_max_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(z_max_label, UITheme::UI_INFO, 0);

    // A-axis info (conditional, hidden by default)
    if (UICommon::isAAxisEnabled()) {
        // A Percentage
        a_percent_label = lv_label_create(info_container);
        lv_label_set_text(a_percent_label, "A: 0%");
        lv_obj_set_style_text_font(a_percent_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(a_percent_label, UITheme::AXIS_A, 0);
        lv_obj_add_flag(a_percent_label, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

        // A Feedrate
        a_feedrate_label = lv_label_create(info_container);
        lv_label_set_text(a_feedrate_label, "0 mm/min");
        lv_obj_set_style_text_font(a_feedrate_label, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(a_feedrate_label, UITheme::TEXT_LIGHT, 0);
        lv_obj_add_flag(a_feedrate_label, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

        // A Max Feedrate (from settings)
        a_max_label = lv_label_create(info_container);
        char a_max_text[32];
        snprintf(a_max_text, sizeof(a_max_text), "Max: %d mm/min", UITabSettingsJog::getMaxAFeed());
        lv_label_set_text(a_max_label, a_max_text);
        lv_obj_set_style_text_font(a_max_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(a_max_label, UITheme::UI_INFO, 0);
        lv_obj_add_flag(a_max_label, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    }

    // ========== Z/A Slider Section (Vertical layout like XY) ==========
    lv_obj_t *za_outer_container = lv_obj_create(parent);
    lv_obj_set_size(za_outer_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(za_outer_container, 5, 0);
    lv_obj_set_flex_flow(za_outer_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(za_outer_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(za_outer_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(za_outer_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(za_outer_container, 0, 0);

    // Z/A Label (centered above slider) - will update based on mode
    za_jog_label = lv_label_create(za_outer_container);
    lv_label_set_text(za_jog_label, "Z JOG");
    lv_obj_set_style_text_font(za_jog_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(za_jog_label, UITheme::AXIS_Z, 0);

    // ========== Z Slider (Vertical) ==========
    z_container = lv_obj_create(za_outer_container);
    lv_obj_set_size(z_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(z_container, 5, 0);
    lv_obj_set_flex_flow(z_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(z_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(z_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(z_container, LV_OPA_TRANSP, 0);  // Transparent background
    lv_obj_set_style_border_width(z_container, 0, 0);  // No border

    // Background slider (80x220 vertical)
    z_bg = lv_obj_create(z_container);
    lv_obj_set_size(z_bg, 80, 220);
    lv_obj_set_style_radius(z_bg, 15, 0);
    lv_obj_set_style_bg_color(z_bg, UITheme::JOYSTICK_BG, 0);
    lv_obj_set_style_border_width(z_bg, 2, 0);
    lv_obj_set_style_border_color(z_bg, UITheme::JOYSTICK_BORDER, 0);
    lv_obj_clear_flag(z_bg, LV_OBJ_FLAG_SCROLLABLE);  // Disable scrolling

    // Center line
    z_line = lv_obj_create(z_bg);
    lv_obj_set_size(z_line, LV_PCT(100), 2);
    lv_obj_set_style_bg_color(z_line, UITheme::JOYSTICK_LINE, 0);
    lv_obj_set_style_border_width(z_line, 0, 0);
    lv_obj_center(z_line);

    // Draggable knob (70x70, purple, centered initially)
    z_knob = lv_obj_create(z_bg);
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

    // ========== A-Axis Slider (Conditional, same position as Z) ==========
    if (UICommon::isAAxisEnabled()) {
        a_container = lv_obj_create(za_outer_container);
        lv_obj_set_size(a_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(a_container, 5, 0);
        lv_obj_set_flex_flow(a_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(a_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(a_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(a_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(a_container, 0, 0);
        lv_obj_add_flag(a_container, LV_OBJ_FLAG_HIDDEN);  // Hidden by default

        // Background slider (80x220 vertical)
        a_bg = lv_obj_create(a_container);
        lv_obj_set_size(a_bg, 80, 220);
        lv_obj_set_style_radius(a_bg, 15, 0);
        lv_obj_set_style_bg_color(a_bg, UITheme::JOYSTICK_BG, 0);
        lv_obj_set_style_border_width(a_bg, 2, 0);
        lv_obj_set_style_border_color(a_bg, UITheme::JOYSTICK_BORDER, 0);
        lv_obj_clear_flag(a_bg, LV_OBJ_FLAG_SCROLLABLE);

        // Center line
        a_line = lv_obj_create(a_bg);
        lv_obj_set_size(a_line, LV_PCT(100), 2);
        lv_obj_set_style_bg_color(a_line, UITheme::JOYSTICK_LINE, 0);
        lv_obj_set_style_border_width(a_line, 0, 0);
        lv_obj_center(a_line);

        // Draggable knob (70x70, orange for A-axis, centered initially)
        a_knob = lv_obj_create(a_bg);
        lv_obj_set_size(a_knob, 70, 70);
        lv_obj_set_style_radius(a_knob, 15, 0);
        lv_obj_set_style_bg_color(a_knob, UITheme::AXIS_A, 0);  // Orange
        lv_obj_set_style_border_width(a_knob, 3, 0);
        lv_obj_set_style_border_color(a_knob, lv_color_white(), 0);
        lv_obj_center(a_knob);

        // Add label to A knob
        lv_obj_t *a_knob_label = lv_label_create(a_knob);
        lv_label_set_text(a_knob_label, "A");
        lv_obj_set_style_text_font(a_knob_label, &lv_font_montserrat_20, 0);
        lv_obj_center(a_knob_label);

        // Add drag event to knob
        lv_obj_add_flag(a_knob, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(a_knob, a_joystick_event_handler, LV_EVENT_PRESSING, NULL);
        lv_obj_add_event_cb(a_knob, a_joystick_event_handler, LV_EVENT_RELEASED, NULL);
    }

    // Z/A Mode selection buttons (below sliders, only if A-axis enabled)
    if (UICommon::isAAxisEnabled()) {
        lv_obj_t *za_button_container = lv_obj_create(za_outer_container);
        lv_obj_set_size(za_button_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(za_button_container, 5, 0);
        lv_obj_set_flex_flow(za_button_container, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(za_button_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_gap(za_button_container, 5, 0);
        lv_obj_clear_flag(za_button_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(za_button_container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(za_button_container, 0, 0);

        // Z button (initially selected with border)
        btn_z_mode = lv_button_create(za_button_container);
        lv_obj_set_size(btn_z_mode, 60, 40);
        lv_obj_set_style_bg_color(btn_z_mode, UITheme::JOYSTICK_Z, LV_PART_MAIN);
        lv_obj_set_style_border_width(btn_z_mode, 3, LV_PART_MAIN);  // Initial selection border
        lv_obj_set_style_border_color(btn_z_mode, lv_color_white(), LV_PART_MAIN);
        lv_obj_t *lbl_z = lv_label_create(btn_z_mode);
        lv_label_set_text(lbl_z, "Z");
        lv_obj_set_style_text_font(lbl_z, &lv_font_montserrat_16, 0);
        lv_obj_center(lbl_z);
        lv_obj_add_event_cb(btn_z_mode, za_joystick_toggle_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)1);

        // A button
        btn_a_mode = lv_button_create(za_button_container);
        lv_obj_set_size(btn_a_mode, 60, 40);
        lv_obj_set_style_bg_color(btn_a_mode, UITheme::AXIS_A, LV_PART_MAIN);
        lv_obj_t *lbl_a = lv_label_create(btn_a_mode);
        lv_label_set_text(lbl_a, "A");
        lv_obj_set_style_text_font(lbl_a, &lv_font_montserrat_16, 0);
        lv_obj_center(lbl_a);
        lv_obj_add_event_cb(btn_a_mode, za_joystick_toggle_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)0);
    }
}
