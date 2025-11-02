#include "ui/tabs/control/ui_tab_control_probe.h"
#include "ui/tabs/settings/ui_tab_settings_probe.h"
#include "ui/ui_theme.h"
#include "network/fluidnc_client.h"
#include "config.h"
#include <lvgl.h>

// Static member for results display
lv_obj_t* UITabControlProbe::results_text = nullptr;

// Keyboard support
lv_obj_t* UITabControlProbe::keyboard = nullptr;
lv_obj_t* UITabControlProbe::parent_tab = nullptr;

// Track which axis was last probed
char UITabControlProbe::last_probed_axis = '\0';

// Static pointers to input fields for reading values
static lv_obj_t* feed_input_ptr = nullptr;
static lv_obj_t* dist_input_ptr = nullptr;
static lv_obj_t* retract_input_ptr = nullptr;
static lv_obj_t* thickness_input_ptr = nullptr;

// Forward declaration for event handler
static void textarea_focused_event_handler(lv_event_t *e);

void UITabControlProbe::create(lv_obj_t *parent) {
    // Store parent tab reference
    parent_tab = parent;
    
    // Load probe defaults from settings
    UITabSettingsProbe::loadPreferences();
    
    // Disable scrolling initially - will be enabled when keyboard appears
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    
    // === X-AXIS SECTION ===
    lv_obj_t* x_header = lv_label_create(parent);
    lv_label_set_text(x_header, "X-AXIS");
    lv_obj_set_style_text_font(x_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(x_header, UITheme::TEXT_DISABLED, 0);  // Same as parameters header
    lv_obj_set_pos(x_header, 10, 10);
    
    // X- button (colored with axis color)
    lv_obj_t* x_minus_btn = lv_button_create(parent);
    lv_obj_set_size(x_minus_btn, 100, 50);  // Narrower buttons
    lv_obj_set_pos(x_minus_btn, 10, 45);
    lv_obj_set_style_bg_color(x_minus_btn, UITheme::AXIS_X, 0);  // Cyan for X-axis
    lv_obj_add_event_cb(x_minus_btn, probe_x_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* x_minus_lbl = lv_label_create(x_minus_btn);
    lv_label_set_text(x_minus_lbl, "X-");
    lv_obj_set_style_text_font(x_minus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(x_minus_lbl);
    
    // X+ button (colored with axis color)
    lv_obj_t* x_plus_btn = lv_button_create(parent);
    lv_obj_set_size(x_plus_btn, 100, 50);  // Narrower buttons
    lv_obj_set_pos(x_plus_btn, 120, 45);  // Adjusted position
    lv_obj_set_style_bg_color(x_plus_btn, UITheme::AXIS_X, 0);  // Cyan for X-axis
    lv_obj_add_event_cb(x_plus_btn, probe_x_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* x_plus_lbl = lv_label_create(x_plus_btn);
    lv_label_set_text(x_plus_lbl, "X+");
    lv_obj_set_style_text_font(x_plus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(x_plus_lbl);
    
    // === Y-AXIS SECTION ===
    lv_obj_t* y_header = lv_label_create(parent);
    lv_label_set_text(y_header, "Y-AXIS");
    lv_obj_set_style_text_font(y_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(y_header, UITheme::TEXT_DISABLED, 0);  // Same as parameters header
    lv_obj_set_pos(y_header, 10, 115);
    
    // Y- button (colored with axis color)
    lv_obj_t* y_minus_btn = lv_button_create(parent);
    lv_obj_set_size(y_minus_btn, 100, 50);  // Narrower buttons
    lv_obj_set_pos(y_minus_btn, 10, 150);
    lv_obj_set_style_bg_color(y_minus_btn, UITheme::AXIS_Y, 0);  // Green for Y-axis
    lv_obj_add_event_cb(y_minus_btn, probe_y_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* y_minus_lbl = lv_label_create(y_minus_btn);
    lv_label_set_text(y_minus_lbl, "Y-");
    lv_obj_set_style_text_font(y_minus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(y_minus_lbl);
    
    // Y+ button (colored with axis color)
    lv_obj_t* y_plus_btn = lv_button_create(parent);
    lv_obj_set_size(y_plus_btn, 100, 50);  // Narrower buttons
    lv_obj_set_pos(y_plus_btn, 120, 150);  // Adjusted position
    lv_obj_set_style_bg_color(y_plus_btn, UITheme::AXIS_Y, 0);  // Green for Y-axis
    lv_obj_add_event_cb(y_plus_btn, probe_y_plus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* y_plus_lbl = lv_label_create(y_plus_btn);
    lv_label_set_text(y_plus_lbl, "Y+");
    lv_obj_set_style_text_font(y_plus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(y_plus_lbl);
    
    // === Z-AXIS SECTION ===
    lv_obj_t* z_header = lv_label_create(parent);
    lv_label_set_text(z_header, "Z-AXIS");
    lv_obj_set_style_text_font(z_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(z_header, UITheme::TEXT_DISABLED, 0);  // Same as parameters header
    lv_obj_set_pos(z_header, 10, 220);
    
    // Z- button (only downward probing makes sense, colored with axis color)
    lv_obj_t* z_minus_btn = lv_button_create(parent);
    lv_obj_set_size(z_minus_btn, 100, 50);  // Narrower button
    lv_obj_set_pos(z_minus_btn, 10, 255);
    lv_obj_set_style_bg_color(z_minus_btn, UITheme::AXIS_Z, 0);  // Magenta for Z-axis
    lv_obj_add_event_cb(z_minus_btn, probe_z_minus_handler, LV_EVENT_CLICKED, NULL);
    lv_obj_t* z_minus_lbl = lv_label_create(z_minus_btn);
    lv_label_set_text(z_minus_lbl, "Z-");
    lv_obj_set_style_text_font(z_minus_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(z_minus_lbl);
    
    // === PARAMETERS SECTION (Right Side) ===
    lv_obj_t* params_header = lv_label_create(parent);
    lv_label_set_text(params_header, "PARAMETERS");
    lv_obj_set_style_text_font(params_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(params_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(params_header, 260, 10);  // Moved left to accommodate wider column
    
    // Feed Rate
    lv_obj_t* feed_label = lv_label_create(parent);
    lv_label_set_text(feed_label, "Feed Rate:");
    lv_obj_set_style_text_font(feed_label, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(feed_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_label, 260, 56);  // Vertically centered with 45px field
    
    feed_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(feed_input_ptr, true);
    lv_obj_set_style_text_font(feed_input_ptr, &lv_font_montserrat_18, 0);  // Increased from 14pt
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", UITabSettingsProbe::getDefaultFeedRate());
    lv_textarea_set_text(feed_input_ptr, buf);
    lv_textarea_set_accepted_chars(feed_input_ptr, "0123456789");  // Integers only
    lv_obj_set_size(feed_input_ptr, 120, 45);  // Wider and taller for 18pt font
    lv_obj_set_pos(feed_input_ptr, 420, 45);  // Shifted left by 30px
    lv_obj_clear_flag(feed_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(feed_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* feed_unit = lv_label_create(parent);
    lv_label_set_text(feed_unit, "mm/min");
    lv_obj_set_style_text_font(feed_unit, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(feed_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(feed_unit, 550, 56);  // Vertically centered with 45px field
    
    // Max Distance
    lv_obj_t* dist_label = lv_label_create(parent);
    lv_label_set_text(dist_label, "Max Distance:");
    lv_obj_set_style_text_font(dist_label, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(dist_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(dist_label, 260, 111);  // Vertically centered with field
    
    dist_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(dist_input_ptr, true);
    lv_obj_set_style_text_font(dist_input_ptr, &lv_font_montserrat_18, 0);  // Increased from 14pt
    snprintf(buf, sizeof(buf), "%d", UITabSettingsProbe::getDefaultMaxDistance());
    lv_textarea_set_text(dist_input_ptr, buf);
    lv_textarea_set_accepted_chars(dist_input_ptr, "0123456789");  // Integers only
    lv_obj_set_size(dist_input_ptr, 120, 45);  // Wider and taller for 18pt font
    lv_obj_set_pos(dist_input_ptr, 420, 100);  // Shifted left by 30px
    lv_obj_clear_flag(dist_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(dist_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* dist_unit = lv_label_create(parent);
    lv_label_set_text(dist_unit, "mm");
    lv_obj_set_style_text_font(dist_unit, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(dist_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(dist_unit, 550, 111);  // Vertically centered with field
    
    // Retract Distance
    lv_obj_t* retract_label = lv_label_create(parent);
    lv_label_set_text(retract_label, "Retract:");
    lv_obj_set_style_text_font(retract_label, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(retract_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(retract_label, 260, 166);  // Vertically centered with field
    
    retract_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(retract_input_ptr, true);
    lv_obj_set_style_text_font(retract_input_ptr, &lv_font_montserrat_18, 0);  // Increased from 14pt
    snprintf(buf, sizeof(buf), "%d", UITabSettingsProbe::getDefaultRetract());
    lv_textarea_set_text(retract_input_ptr, buf);
    lv_textarea_set_accepted_chars(retract_input_ptr, "0123456789");  // Integers only
    lv_obj_set_size(retract_input_ptr, 120, 45);  // Wider and taller for 18pt font
    lv_obj_set_pos(retract_input_ptr, 420, 155);  // Shifted left by 30px
    lv_obj_clear_flag(retract_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(retract_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* retract_unit = lv_label_create(parent);
    lv_label_set_text(retract_unit, "mm");
    lv_obj_set_style_text_font(retract_unit, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(retract_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(retract_unit, 550, 166);  // Vertically centered with field
    
    // Probe Thickness (P parameter)
    lv_obj_t* thickness_label = lv_label_create(parent);
    lv_label_set_text(thickness_label, "Probe Thickness:");
    lv_obj_set_style_text_font(thickness_label, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(thickness_label, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(thickness_label, 260, 221);  // Vertically centered with field
    
    thickness_input_ptr = lv_textarea_create(parent);
    lv_textarea_set_one_line(thickness_input_ptr, true);
    lv_obj_set_style_text_font(thickness_input_ptr, &lv_font_montserrat_18, 0);  // Increased from 14pt
    snprintf(buf, sizeof(buf), "%.1f", UITabSettingsProbe::getDefaultThickness());
    lv_textarea_set_text(thickness_input_ptr, buf);
    lv_textarea_set_accepted_chars(thickness_input_ptr, "0123456789.");  // Allow decimal point
    lv_obj_set_size(thickness_input_ptr, 120, 45);  // Wider and taller for 18pt font
    lv_obj_set_pos(thickness_input_ptr, 420, 210);  // Shifted left by 30px
    lv_obj_clear_flag(thickness_input_ptr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(thickness_input_ptr, textarea_focused_event_handler, LV_EVENT_FOCUSED, nullptr);
    
    lv_obj_t* thickness_unit = lv_label_create(parent);
    lv_label_set_text(thickness_unit, "mm");
    lv_obj_set_style_text_font(thickness_unit, &lv_font_montserrat_18, 0);  // Increased from 14pt
    lv_obj_set_style_text_color(thickness_unit, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_pos(thickness_unit, 550, 221);  // Vertically centered with field
    
    // === RESULTS SECTION ===
    lv_obj_t* results_header = lv_label_create(parent);
    lv_label_set_text(results_header, "PROBE RESULT");
    lv_obj_set_style_text_font(results_header, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(results_header, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(results_header, 260, 260);  // Aligned with parameters section, moved down 20px
    
    results_text = lv_textarea_create(parent);
    lv_textarea_set_text(results_text, "No probe data");
    lv_obj_set_size(results_text, 370, 50);  // 10px wider for better readability
    lv_obj_set_pos(results_text, 260, 295);  // Aligned with parameters section, moved down 20px
    lv_obj_set_style_text_font(results_text, &lv_font_montserrat_16, 0);  // Larger font for better readability
    lv_obj_set_style_pad_all(results_text, 3, 0);  // Reduced padding to fit 2 lines comfortably
    lv_obj_clear_flag(results_text, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(results_text, LV_OBJ_FLAG_SCROLLABLE);  // Prevent scrolling
}

// Event handlers
void UITabControlProbe::probe_x_minus_handler(lv_event_t* e) {
    executeProbe("X", "-");
}

void UITabControlProbe::probe_x_plus_handler(lv_event_t* e) {
    executeProbe("X", "+");
}

void UITabControlProbe::probe_y_minus_handler(lv_event_t* e) {
    executeProbe("Y", "-");
}

void UITabControlProbe::probe_y_plus_handler(lv_event_t* e) {
    executeProbe("Y", "+");
}

void UITabControlProbe::probe_z_minus_handler(lv_event_t* e) {
    executeProbe("Z", "-");
}

void UITabControlProbe::executeProbe(const char* axis, const char* direction) {
    if (!FluidNCClient::isConnected()) {
        if (results_text) {
            lv_textarea_set_text(results_text, "Error: Not connected to FluidNC");
        }
        return;
    }
    
    // Store which axis is being probed for result filtering
    last_probed_axis = axis[0];
    
    // Read parameters from input fields
    const char* feed_text = lv_textarea_get_text(feed_input_ptr);
    const char* dist_text = lv_textarea_get_text(dist_input_ptr);
    const char* retract_text = lv_textarea_get_text(retract_input_ptr);
    const char* thickness_text = lv_textarea_get_text(thickness_input_ptr);
    
    float feed_rate = atof(feed_text);
    float max_dist = atof(dist_text);
    float retract = atof(retract_text);
    float thickness = atof(thickness_text);
    
    // Validate inputs
    if (feed_rate <= 0 || max_dist <= 0) {
        if (results_text) {
            lv_textarea_set_text(results_text, "Error: Invalid feed rate or distance");
        }
        return;
    }
    
    // Update result text to show probing in progress
    if (results_text) {
        char status[64];
        snprintf(status, sizeof(status), "Probing %s%s...", axis, direction);
        lv_textarea_set_text(results_text, status);
    }
    
    // Save current distance mode (G90/G91) to restore after probing
    const char* saved_distance_mode = FluidNCClient::getStatus().modal_distance;
    
    // Set incremental mode BEFORE probe command (modal group violation if combined)
    FluidNCClient::sendCommand("G91\n");
    
    // Build G38.2 command WITHOUT G91 (already set above): G38.2 <AXIS><DIRECTION><DISTANCE> F<FEED> P<THICKNESS>
    char command[128];
    
    if (thickness > 0.001) {  // Use P parameter if thickness is specified
        snprintf(command, sizeof(command), "G38.2 %c%.1f F%.0f P%.2f\n", 
                 axis[0], (direction[0] == '-' ? -max_dist : max_dist), feed_rate, thickness);
    } else {
        snprintf(command, sizeof(command), "G38.2 %c%.1f F%.0f\n", 
                 axis[0], (direction[0] == '-' ? -max_dist : max_dist), feed_rate);
    }
    
    Serial.printf("Probe: Sending command: %s\n", command);
    FluidNCClient::sendCommand(command);
    
    // Send retract move if retract distance is specified
    if (retract > 0.001) {
        // Retract in opposite direction from probe (no + sign in GCode)
        char retract_sign = (direction[0] == '-') ? ' ' : '-';
        
        // Still in G91 mode from probe setup, send retract move
        char retract_command[64];
        snprintf(retract_command, sizeof(retract_command), "G0 %c%c%.2f\n", 
                 axis[0], retract_sign, retract);
        Serial.printf("Probe: Sending retract: %s", retract_command);
        FluidNCClient::sendCommand(retract_command);
    }
    
    // Restore original distance mode (G90 or G91)
    char restore_cmd[8];
    snprintf(restore_cmd, sizeof(restore_cmd), "%s\n", saved_distance_mode);
    FluidNCClient::sendCommand(restore_cmd);
}

void UITabControlProbe::updateResult(const char* message) {
    if (results_text && message) {
        lv_textarea_set_text(results_text, message);
    }
}

void UITabControlProbe::updateResult(float x, float y, float z, bool success) {
    if (!results_text) return;
    
    char result[256];
    if (success) {
        // Only show the value for the axis that was probed
        const char* axis_name = "";
        float axis_value = 0.0f;
        
        switch (last_probed_axis) {
            case 'X':
                axis_name = "X";
                axis_value = x;
                break;
            case 'Y':
                axis_name = "Y";
                axis_value = y;
                break;
            case 'Z':
                axis_name = "Z";
                axis_value = z;
                break;
            default:
                // Fallback: show all axes if unknown (compact 2-line format)
                snprintf(result, sizeof(result), 
                        "SUCCESS\nX:%.3f Y:%.3f Z:%.3f", 
                        x, y, z);
                lv_textarea_set_text(results_text, result);
                return;
        }
        
        // Compact 2-line format
        snprintf(result, sizeof(result), 
                "SUCCESS\n%s: %.3f mm", 
                axis_name, axis_value);
    } else {
        // Compact 2-line format
        snprintf(result, sizeof(result), 
                "FAILED\nNo contact detected");
    }
    
    lv_textarea_set_text(results_text, result);
}

// Textarea focused event handler - show keyboard
static void textarea_focused_event_handler(lv_event_t *e) {
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    UITabControlProbe::showKeyboard(ta);
}

// Show keyboard
void UITabControlProbe::showKeyboard(lv_obj_t *ta) {
    if (!keyboard) {
        keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(keyboard, SCREEN_WIDTH, 220);
        lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);  // Numeric keyboard for probe parameters
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlProbe::hideKeyboard(); }, LV_EVENT_READY, nullptr);
        lv_obj_add_event_cb(keyboard, [](lv_event_t *e) { UITabControlProbe::hideKeyboard(); }, LV_EVENT_CANCEL, nullptr);
        if (parent_tab) {
            lv_obj_add_event_cb(parent_tab, [](lv_event_t *e) { UITabControlProbe::hideKeyboard(); }, LV_EVENT_CLICKED, nullptr);
        }
    }
    
    // Enable scrolling on parent tab and add extra padding at bottom for keyboard (every time keyboard is shown)
    if (parent_tab) {
        lv_obj_add_flag(parent_tab, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_bottom(parent_tab, 240, 0); // Extra space for scrolling (keyboard height + margin)
    }
    
    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll the parent tab to position the focused textarea just above keyboard
    if (parent_tab && ta) {
        // Get textarea position within parent_tab
        lv_coord_t ta_y = lv_obj_get_y(ta);
        lv_obj_t *parent = lv_obj_get_parent(ta);
        
        // Walk up parent hierarchy to get cumulative Y position
        while (parent && parent != parent_tab) {
            ta_y += lv_obj_get_y(parent);
            parent = lv_obj_get_parent(parent);
        }
        
        // Calculate scroll position to place textarea just above keyboard
        // Status bar is 60px, keyboard is 220px, so visible area is 200px (480 - 60 - 220)
        lv_coord_t visible_height = 200; // Height above keyboard and below status bar
        lv_coord_t ta_height = lv_obj_get_height(ta);
        lv_coord_t target_position = visible_height - ta_height - 10; // 10px margin above keyboard
        
        // Scroll amount = (textarea Y position) - (where we want it)
        lv_coord_t scroll_y = ta_y - target_position;
        if (scroll_y < 0) scroll_y = 0; // Don't scroll past top
        
        lv_obj_scroll_to_y(parent_tab, scroll_y, LV_ANIM_ON);
    }
}

// Hide keyboard
void UITabControlProbe::hideKeyboard() {
    if (keyboard) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        
        // Restore parent tab to non-scrollable and remove extra padding
        if (parent_tab) {
            lv_obj_clear_flag(parent_tab, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_pad_bottom(parent_tab, 10, 0); // Back to original padding
            lv_obj_scroll_to_y(parent_tab, 0, LV_ANIM_ON); // Reset scroll position
        }
    }
}
