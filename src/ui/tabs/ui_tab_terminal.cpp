#include "ui/tabs/ui_tab_terminal.h"
#include "ui/ui_theme.h"
#include "network/fluidnc_client.h"
#include "config.h"
#include "ui/fonts/jetbrains_mono_16.h"

// Static member initialization
lv_obj_t *UITabTerminal::terminal_text = nullptr;
lv_obj_t *UITabTerminal::terminal_cont = nullptr;
lv_obj_t *UITabTerminal::input_field = nullptr;
lv_obj_t *UITabTerminal::keyboard = nullptr;
lv_obj_t *UITabTerminal::auto_scroll_switch = nullptr;
String UITabTerminal::terminal_buffer = "";
bool UITabTerminal::auto_scroll_enabled = true;
bool UITabTerminal::buffer_dirty = false;
uint32_t UITabTerminal::last_update_ms = 0;
bool UITabTerminal::in_json_message = false;
int UITabTerminal::json_brace_count = 0;
String UITabTerminal::cmd_history[10];
int UITabTerminal::hist_count = 0;
int UITabTerminal::hist_index = -1;
String UITabTerminal::hist_draft = "";

void UITabTerminal::create(lv_obj_t *tab) {
    // Set 5px margins by using padding
    lv_obj_set_style_pad_all(tab, 15, 0);

    // Set dark background
    lv_obj_set_style_bg_color(tab, UITheme::BG_MEDIUM, LV_PART_MAIN);

    // Calculate available height for content
    // Tab content height = SCREEN_HEIGHT (480) - STATUS_BAR_HEIGHT (60) - TAB_BUTTON_HEIGHT (50) = 370px
    const int content_height = SCREEN_HEIGHT - STATUS_BAR_HEIGHT - TAB_BUTTON_HEIGHT;
    const int input_height = 45;
    const int btn_small_w = 45;   // Clear / Up / Down buttons
    const int btn_send_w = 80;    // Send button
    const int gap = 5;
    // Effective content width: 800 - 2*15 padding = 770px
    // Right 120px reserved for auto-scroll; buttons: 3*45 + 80 + 4*5 = 235px; input: 415px
    const int input_width = 770 - 120 - (3 * btn_small_w) - btn_send_w - (4 * gap);
    const int terminal_height = content_height - input_height - (gap * 3);

    // Input text area
    input_field = lv_textarea_create(tab);
    lv_obj_set_size(input_field, input_width, input_height);
    lv_obj_set_pos(input_field, 0, 0);
    lv_textarea_set_one_line(input_field, true);
    lv_textarea_set_placeholder_text(input_field, "Enter command...");
    lv_obj_set_style_text_font(input_field, &lv_font_montserrat_18, 0);
    lv_obj_set_style_bg_color(input_field, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_set_style_text_color(input_field, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(input_field, input_field_event_cb, LV_EVENT_CLICKED, nullptr);

    int bx = input_width + gap;

    // Clear (X) button
    lv_obj_t *clear_btn = lv_button_create(tab);
    lv_obj_set_size(clear_btn, btn_small_w, input_height);
    lv_obj_set_pos(clear_btn, bx, 0);
    lv_obj_set_style_bg_color(clear_btn, UITheme::BTN_DISCONNECT, LV_PART_MAIN);
    lv_obj_add_event_cb(clear_btn, clear_btn_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *clear_lbl = lv_label_create(clear_btn);
    lv_label_set_text(clear_lbl, LV_SYMBOL_CLOSE);
    lv_obj_center(clear_lbl);
    bx += btn_small_w + gap;

    // History up button (older command)
    lv_obj_t *hist_up = lv_button_create(tab);
    lv_obj_set_size(hist_up, btn_small_w, input_height);
    lv_obj_set_pos(hist_up, bx, 0);
    lv_obj_set_style_bg_color(hist_up, UITheme::ACCENT_SECONDARY, LV_PART_MAIN);
    lv_obj_add_event_cb(hist_up, hist_up_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *up_lbl = lv_label_create(hist_up);
    lv_label_set_text(up_lbl, LV_SYMBOL_UP);
    lv_obj_center(up_lbl);
    bx += btn_small_w + gap;

    // History down button (newer command)
    lv_obj_t *hist_down = lv_button_create(tab);
    lv_obj_set_size(hist_down, btn_small_w, input_height);
    lv_obj_set_pos(hist_down, bx, 0);
    lv_obj_set_style_bg_color(hist_down, UITheme::ACCENT_SECONDARY, LV_PART_MAIN);
    lv_obj_add_event_cb(hist_down, hist_down_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *down_lbl = lv_label_create(hist_down);
    lv_label_set_text(down_lbl, LV_SYMBOL_DOWN);
    lv_obj_center(down_lbl);
    bx += btn_small_w + gap;

    // Send button
    lv_obj_t *send_btn = lv_button_create(tab);
    lv_obj_set_size(send_btn, btn_send_w, input_height);
    lv_obj_set_pos(send_btn, bx, 0);
    lv_obj_set_style_bg_color(send_btn, UITheme::ACCENT_PRIMARY, LV_PART_MAIN);
    lv_obj_add_event_cb(send_btn, send_button_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *send_label = lv_label_create(send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_set_style_text_font(send_label, &lv_font_montserrat_18, 0);
    lv_obj_center(send_label);

    // Auto-scroll toggle (switch + label) on the right
    lv_obj_t *auto_scroll_label = lv_label_create(tab);
    lv_label_set_text(auto_scroll_label, "Auto Scroll");
    lv_obj_set_style_text_font(auto_scroll_label, &lv_font_montserrat_14, 0);
    lv_obj_align(auto_scroll_label, LV_ALIGN_TOP_RIGHT, -5, 0);

    auto_scroll_switch = lv_switch_create(tab);
    lv_obj_set_size(auto_scroll_switch, 50, 25);
    lv_obj_align(auto_scroll_switch, LV_ALIGN_TOP_RIGHT, -20, 18);
    lv_obj_add_state(auto_scroll_switch, LV_STATE_CHECKED);  // Start enabled
    lv_obj_add_event_cb(auto_scroll_switch, auto_scroll_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Terminal output container
    terminal_cont = lv_obj_create(tab);
    lv_obj_set_size(terminal_cont, 770, 270);
    lv_obj_set_pos(terminal_cont, 0, 60);
    lv_obj_set_style_bg_color(terminal_cont, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_set_style_border_color(terminal_cont, UITheme::BORDER_LIGHT, LV_PART_MAIN);
    lv_obj_set_style_border_width(terminal_cont, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(terminal_cont, 5, LV_PART_MAIN);
    // Enable scrolling for terminal output
    lv_obj_set_scroll_dir(terminal_cont, LV_DIR_VER);

    terminal_text = lv_label_create(terminal_cont);
    lv_label_set_text(terminal_text, "");
    lv_obj_set_style_text_font(terminal_text, &jetbrains_mono_16, 0);
    lv_obj_set_style_text_color(terminal_text, UITheme::UI_SUCCESS, 0);
    lv_label_set_long_mode(terminal_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(terminal_text, 760);

    // Initialize buffer (empty)
    terminal_buffer = "";
}

// Send button event handler
void UITabTerminal::send_button_event_cb(lv_event_t *e) {
    send_command();
}

// Input field event handler - show keyboard when clicked
void UITabTerminal::input_field_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        // Create keyboard if it doesn't exist
        if (keyboard == nullptr) {
            keyboard = lv_keyboard_create(lv_screen_active());
            lv_keyboard_set_textarea(keyboard, input_field);
            lv_obj_set_size(keyboard, SCREEN_WIDTH, 240);
            lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_20, 0);  // Larger font for better visibility
            lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_READY, nullptr);
            lv_obj_add_event_cb(keyboard, keyboard_event_cb, LV_EVENT_CANCEL, nullptr);
        } else {
            // Show existing keyboard
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Keyboard event handler
void UITabTerminal::keyboard_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) {
        // User pressed "Enter" or "OK"
        send_command();
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_CANCEL) {
        // User pressed "Cancel"
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

// Auto-scroll event handler
void UITabTerminal::auto_scroll_event_cb(lv_event_t *e) {
    lv_obj_t *sw = (lv_obj_t *)lv_event_get_target(e);
    auto_scroll_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    Serial.printf("[Terminal] Auto-scroll %s\n", auto_scroll_enabled ? "enabled" : "disabled");
}

// Send command function
void UITabTerminal::send_command() {
    const char *cmd = lv_textarea_get_text(input_field);

    if (cmd != nullptr && strlen(cmd) > 0) {
        String cmd_str = String(cmd);

        // Add to history (most recent first, skip duplicate consecutive entry)
        if (hist_count == 0 || cmd_history[0] != cmd_str) {
            for (int i = 9; i > 0; i--) {
                cmd_history[i] = cmd_history[i - 1];
            }
            cmd_history[0] = cmd_str;
            if (hist_count < 10) hist_count++;
        }
        hist_index = -1;
        hist_draft = "";

        // Log to serial for debugging
        Serial.print("[Terminal] Sending command: ");
        Serial.println(cmd_str.c_str());

        // Send command to FluidNC with newline
        FluidNCClient::sendCommand((cmd_str + "\n").c_str());

        // Echo command to terminal
        terminal_buffer += "> ";
        terminal_buffer += cmd_str;
        terminal_buffer += "\n";
        trimBuffer();

        // Update display immediately for user commands
        updateDisplay();
        last_update_ms = millis();  // Reset timer

        // Clear input field
        lv_textarea_set_text(input_field, "");
    }

    // Hide keyboard after sending
    if (keyboard != nullptr) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void UITabTerminal::appendMessage(const char *message) {
    if (message == nullptr || strlen(message) == 0) return;

    // Filter out status messages (those starting with '<')
    // These cause too many updates and performance issues
    if (message[0] == '<') {
        return;
    }
    
    // Filter out ping messages (those starting with 'PING:')
    if (strncmp(message, "PING:", 5) == 0) {
        return;
    }
    
    // Filter out "ok" messages
    if (strcmp(message, "ok") == 0 || strncmp(message, "ok\n", 3) == 0 || strncmp(message, "ok\r", 3) == 0) {
        return;
    }
    
    // Track JSON message state by counting braces
    // If we see '{', we're entering a JSON message
    if (message[0] == '{') {
        in_json_message = true;
        json_brace_count = 0;
    }
    
    // If we're inside a JSON message, count braces to track when it ends
    if (in_json_message) {
        for (size_t i = 0; i < strlen(message); i++) {
            if (message[i] == '{') {
                json_brace_count++;
            } else if (message[i] == '}') {
                json_brace_count--;
                // If brace count reaches 0, JSON message is complete
                if (json_brace_count <= 0) {
                    in_json_message = false;
                    json_brace_count = 0;
                }
            }
        }
        // Filter out this line (it's part of JSON)
        return;
    }
    
    // Filter out CURRENT_ID and ACTIVE_ID messages (prevent performance issues during jogging)
    if (strncmp(message, "CURRENT_ID:", 11) == 0 || strncmp(message, "ACTIVE_ID:", 10) == 0) {
        return;
    }

    // Strip trailing whitespace/newlines from message
    String clean_msg = String(message);
    while (clean_msg.length() > 0 && isspace(clean_msg[clean_msg.length() - 1])) {
        clean_msg.remove(clean_msg.length() - 1);
    }

    // Skip blank lines - check if cleaned message is empty
    if (clean_msg.length() == 0) {
        return;
    }

    // Append cleaned message to buffer with exactly one newline
    terminal_buffer += clean_msg;
    terminal_buffer += "\n";

    // Trim buffer if needed
    trimBuffer();

    // Mark buffer as dirty - UI will be updated in update() method
    buffer_dirty = true;
}

void UITabTerminal::trimBuffer() {
    // If buffer exceeds max size, remove lines from the beginning
    if (terminal_buffer.length() > MAX_BUFFER_SIZE) {
        // Find position to start trimming (keep last 75% of buffer)
        size_t trim_to = MAX_BUFFER_SIZE * 3 / 4;

        // Find the first newline after the trim point to avoid cutting mid-line
        int newline_pos = terminal_buffer.indexOf('\n', terminal_buffer.length() - trim_to);

        if (newline_pos > 0) {
            terminal_buffer = terminal_buffer.substring(newline_pos + 1);
        } else {
            // No newline found, just trim to size
            terminal_buffer = terminal_buffer.substring(terminal_buffer.length() - trim_to);
        }

        Serial.printf("[Terminal] Buffer trimmed to %d bytes\n", terminal_buffer.length());
    }
}

void UITabTerminal::update() {
    // Check if enough time has passed since last update
    uint32_t now = millis();
    if (now - last_update_ms < UPDATE_INTERVAL_MS) {
        return;
    }

    // Only update if buffer has changed
    if (buffer_dirty) {
        updateDisplay();
        buffer_dirty = false;
        last_update_ms = now;
    }
}

void UITabTerminal::updateDisplay() {
    // Update display if terminal exists
    if (terminal_text) {
        lv_label_set_text(terminal_text, terminal_buffer.c_str());

        // Auto-scroll to bottom only if enabled
        if (terminal_cont && auto_scroll_enabled) {
            lv_obj_scroll_to_y(terminal_cont, LV_COORD_MAX, LV_ANIM_OFF);
        }
    }
}

void UITabTerminal::clear_btn_event_cb(lv_event_t *e) {
    lv_textarea_set_text(input_field, "");
    hist_index = -1;
}

void UITabTerminal::hist_up_event_cb(lv_event_t *e) {
    if (hist_count == 0) return;
    if (hist_index == -1) {
        hist_draft = String(lv_textarea_get_text(input_field));
        hist_index = 0;
    } else if (hist_index < hist_count - 1) {
        hist_index++;
    }
    lv_textarea_set_text(input_field, cmd_history[hist_index].c_str());
}

void UITabTerminal::hist_down_event_cb(lv_event_t *e) {
    if (hist_index == -1) return;
    if (hist_index > 0) {
        hist_index--;
        lv_textarea_set_text(input_field, cmd_history[hist_index].c_str());
    } else {
        hist_index = -1;
        lv_textarea_set_text(input_field, hist_draft.c_str());
    }
}
