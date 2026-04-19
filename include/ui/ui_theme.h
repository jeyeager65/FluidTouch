#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

// FluidTouch UI Theme Colors
// Using constexpr functions for C++11 compatibility
namespace UITheme {
    // Background Colors
    static constexpr lv_color_t BG_DARK = LV_COLOR_MAKE(0x0a, 0x0a, 0x0a);      // Status bar, darkest
    static constexpr lv_color_t BG_DARKER = LV_COLOR_MAKE(0x1a, 0x1a, 0x1a);    // Main screen background
    static constexpr lv_color_t BG_MEDIUM = LV_COLOR_MAKE(0x2a, 0x2a, 0x2a);    // Content areas, tab backgrounds
    static constexpr lv_color_t BG_BUTTON = LV_COLOR_MAKE(0x33, 0x33, 0x33);    // Input fields, unselected buttons
    static constexpr lv_color_t BG_BLACK = LV_COLOR_MAKE(0x00, 0x00, 0x00);     // Terminal background
    static constexpr lv_color_t BG_PROGRESS = LV_COLOR_MAKE(0x88, 0x88, 0x8);   // Progress Bar background
    
    // Primary Accent Colors
    static constexpr lv_color_t ACCENT_PRIMARY = LV_COLOR_MAKE(0x00, 0x78, 0xD7);      // Main tab selected (blue)
    static constexpr lv_color_t ACCENT_PRIMARY_PRESSED = LV_COLOR_MAKE(0x00, 0x5F, 0xA3); // Primary pressed state (darker blue)
    static constexpr lv_color_t ACCENT_SECONDARY = LV_COLOR_MAKE(0x00, 0xAA, 0x88);      // Sub-tab selected (teal)
    
    // Text Colors
    static constexpr lv_color_t TEXT_LIGHT = LV_COLOR_MAKE(0xCC, 0xCC, 0xCC);   // Light gray text
    static constexpr lv_color_t TEXT_MEDIUM = LV_COLOR_MAKE(0xAA, 0xAA, 0xAA);  // Medium gray text
    static constexpr lv_color_t TEXT_DARK = LV_COLOR_MAKE(0x55, 0x55, 0x55);    // Dark gray text (borders, pressed buttons)
    
    // Machine Status Colors
    static constexpr lv_color_t STATE_IDLE = LV_COLOR_MAKE(0x00, 0xFF, 0x00);   // Idle state, OK status (green)
    static constexpr lv_color_t STATE_RUN = LV_COLOR_MAKE(0x00, 0xBF, 0xFF);    // Running state (cyan)
    static constexpr lv_color_t STATE_ALARM = LV_COLOR_MAKE(0xFF, 0x00, 0x00);  // Alarm/Error state (red)
    static constexpr lv_color_t STATE_HOLD = LV_COLOR_MAKE(0xFB, 0xC0, 0x2D);   // Hold/Pause state (dark yellow)
    static constexpr lv_color_t STATE_UNKNOWN = LV_COLOR_MAKE(0xCC, 0xCC, 0xCC);// Unknown state (gray)
    
    // Axis Colors
    static constexpr lv_color_t AXIS_X = LV_COLOR_MAKE(0x00, 0x6F, 0xAA);       // X-axis (darker blue-cyan)
    static constexpr lv_color_t AXIS_Y = LV_COLOR_MAKE(0x00, 0xAA, 0x00);       // Y-axis (darker green)
    static constexpr lv_color_t AXIS_Z = LV_COLOR_MAKE(0x9B, 0x59, 0xB6);       // Z-axis (purple)
    static constexpr lv_color_t AXIS_A = LV_COLOR_MAKE(0xFF, 0x8C, 0x00);       // A-axis (orange)
    static constexpr lv_color_t AXIS_XY = LV_COLOR_MAKE(0x00, 0xAA, 0x88);      // XY combined (teal, between cyan and green)
    
    // Position Display Colors
    static constexpr lv_color_t POS_MACHINE = LV_COLOR_MAKE(0x00, 0xBF, 0xFF);  // Machine position label (cyan)
    static constexpr lv_color_t POS_WORK = LV_COLOR_MAKE(0xFF, 0x8C, 0x00);     // Work position label (orange)
    static constexpr lv_color_t POS_MODAL = LV_COLOR_MAKE(0xFF, 0xFF, 0x00);    // Modal states (yellow)
    
    // Border Colors
    static constexpr lv_color_t BORDER_MEDIUM = LV_COLOR_MAKE(0x44, 0x44, 0x44);// Status bar border
    static constexpr lv_color_t BORDER_LIGHT = LV_COLOR_MAKE(0x55, 0x55, 0x55); // Terminal border
    
    // Extended Colors (for special widgets and indicators)
    static constexpr lv_color_t DROPDOWN_BG = LV_COLOR_MAKE(0x40, 0x40, 0x40);  // Dropdown backgrounds
    static constexpr lv_color_t FILE_ITEM_BORDER = LV_COLOR_MAKE(0x40, 0x40, 0x40); // File entry borders
    static constexpr lv_color_t FILE_SIZE_TEXT = LV_COLOR_MAKE(0x88, 0x88, 0x88); // File size gray text
    static constexpr lv_color_t TEXT_DISABLED = LV_COLOR_MAKE(0x66, 0x66, 0x66); // Disabled/secondary text
    
    // Button Colors (special actions)
    static constexpr lv_color_t BTN_PLAY = LV_COLOR_MAKE(0x4C, 0xAF, 0x50);     // Play/Success buttons (green)
    static constexpr lv_color_t BTN_PROBE = LV_COLOR_MAKE(0x00, 0xAA, 0x00);    // Probe button (dark green)
    static constexpr lv_color_t BTN_CONNECT = LV_COLOR_MAKE(0x21, 0x96, 0xF3);  // Connect button (blue)
    static constexpr lv_color_t BTN_DISCONNECT = LV_COLOR_MAKE(0xFF, 0x57, 0x22); // Disconnect button (orange-red)
    static constexpr lv_color_t BTN_ESTOP = LV_COLOR_MAKE(0xB7, 0x1C, 0x1C);    // Emergency stop (dark red)
    
    // Metric Card Colors
    static constexpr lv_color_t METRIC_UPTIME = LV_COLOR_MAKE(0x21, 0x96, 0xF3);    // Uptime metric (blue)
    static constexpr lv_color_t METRIC_MEMORY = LV_COLOR_MAKE(0x9C, 0x27, 0xB0);    // PSRAM metric (purple)
    static constexpr lv_color_t METRIC_HEAP = LV_COLOR_MAKE(0xFF, 0x98, 0x00);      // Heap metric (orange)
    static constexpr lv_color_t METRIC_WIFI = LV_COLOR_MAKE(0x4C, 0xAF, 0x50);      // WiFi signal metric (green)
    
    // LED/Indicator Colors
    static constexpr lv_color_t LED_OFF = LV_COLOR_MAKE(0x66, 0x66, 0x66);      // LED inactive
    
    // Battery Indicator Colors
    static constexpr lv_color_t BATTERY_FULL = LV_COLOR_MAKE(0x4C, 0xAF, 0x50);      // Green (>55%)
    static constexpr lv_color_t BATTERY_MID = LV_COLOR_MAKE(0xFF, 0xB7, 0x4D);       // Amber (20-55%)
    static constexpr lv_color_t BATTERY_LOW = LV_COLOR_MAKE(0xFF, 0x00, 0x00);       // Red (<20%)
    static constexpr lv_color_t BATTERY_CHARGING = LV_COLOR_MAKE(0x00, 0xBF, 0xFF);  // Cyan (charging)

    // Joystick/Control Colors
    static constexpr lv_color_t JOYSTICK_BG = LV_COLOR_MAKE(0x30, 0x30, 0x30);  // Joystick background
    static constexpr lv_color_t JOYSTICK_LINE = LV_COLOR_MAKE(0x60, 0x60, 0x60); // Joystick crosshairs
    static constexpr lv_color_t JOYSTICK_BORDER = LV_COLOR_MAKE(0x80, 0x80, 0x80); // Joystick border
    static constexpr lv_color_t JOYSTICK_XY = LV_COLOR_MAKE(0x00, 0xAA, 0x88);  // XY-axis joystick knob (teal)
    static constexpr lv_color_t JOYSTICK_Z = LV_COLOR_MAKE(0x9B, 0x59, 0xB6);   // Z-axis joystick knob (purple)
    static constexpr lv_color_t PROBE_RESULTS_BG = LV_COLOR_MAKE(0x22, 0x22, 0x22); // Probe results background
    
    // UI Feedback Colors
    static constexpr lv_color_t UI_SUCCESS = LV_COLOR_MAKE(0x00, 0xFF, 0x00);   // Success messages (green)
    static constexpr lv_color_t UI_WARNING = LV_COLOR_MAKE(0xFB, 0x8C, 0x00);   // Warning messages (orange)
    static constexpr lv_color_t UI_INFO = LV_COLOR_MAKE(0x00, 0xBC, 0xD4);      // Info text, section titles (cyan)
    static constexpr lv_color_t UI_NOTE = LV_COLOR_MAKE(0xFF, 0xB7, 0x4D);      // Helpful notes (amber)
    static constexpr lv_color_t UI_SECONDARY = LV_COLOR_MAKE(0x00, 0xFF, 0xFF); // Secondary info (bright cyan)
    
    // Macro Button Colors (predefined palette) - Rainbow order (darker shades)
    static constexpr lv_color_t MACRO_COLOR_1 = LV_COLOR_MAKE(0xCC, 0x00, 0x00); // Bright Red
    static constexpr lv_color_t MACRO_COLOR_2 = LV_COLOR_MAKE(0xCC, 0x44, 0x00); // Darker Orange
    static constexpr lv_color_t MACRO_COLOR_3 = LV_COLOR_MAKE(0xF5, 0x7F, 0x17); // Dark Yellow
    static constexpr lv_color_t MACRO_COLOR_4 = LV_COLOR_MAKE(0x2E, 0x7D, 0x32); // Dark Green
    static constexpr lv_color_t MACRO_COLOR_5 = LV_COLOR_MAKE(0x00, 0x83, 0x8F); // Dark Cyan
    static constexpr lv_color_t MACRO_COLOR_6 = LV_COLOR_MAKE(0x00, 0x1F, 0x5C); // Navy Blue
    static constexpr lv_color_t MACRO_COLOR_7 = LV_COLOR_MAKE(0x6A, 0x1B, 0x9A); // Dark Purple
    static constexpr lv_color_t MACRO_COLOR_8 = LV_COLOR_MAKE(0x45, 0x45, 0x45); // Dark Gray
}

#endif // UI_THEME_H
