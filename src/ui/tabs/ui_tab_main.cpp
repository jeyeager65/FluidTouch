#include "ui/tabs/ui_tab_main.h"
#include "ui/ui_theme.h"
#include "ui/ui_common.h"
#include "ui/wcs_config.h"
#include "ui/tabs/settings/ui_tab_settings_jog.h"
#include "ui/machine_config.h"
#include "network/fluidnc_client.h"
#include "config.h"
#include <Preferences.h>

// Static member initialization
lv_obj_t *UITabMain::lbl_state = nullptr;
lv_obj_t *UITabMain::lbl_message = nullptr;
lv_obj_t *UITabMain::ind_limit_x = nullptr;
lv_obj_t *UITabMain::lbl_wpos_x = nullptr;
lv_obj_t *UITabMain::lbl_mpos_x = nullptr;
float UITabMain::last_wpos_x = -9999.0f;
float UITabMain::last_mpos_x = -9999.0f;
char UITabMain::last_state[16] = "";
uint32_t UITabMain::last_limit_trigger_x_ms = 0;
lv_obj_t *UITabMain::btn_home_x = nullptr;
lv_obj_t *UITabMain::keyboard = nullptr;
lv_obj_t *UITabMain::active_textarea = nullptr;
char UITabMain::original_value[32] = "";
float UITabMain::step_values[UITabMain::MAX_STEPS] = {};
int UITabMain::step_count = 0;
int UITabMain::step_index = 0;
lv_obj_t *UITabMain::step_buttons[UITabMain::MAX_STEPS] = {};
float UITabMain::saved_positions[UITabMain::MAX_SAVED] = {};
int UITabMain::saved_count = 0;
lv_obj_t *UITabMain::saved_buttons[UITabMain::MAX_SAVED] = {};
lv_obj_t *UITabMain::lbl_saved_hint = nullptr;
int UITabMain::pending_delete_index = -1;
uint32_t UITabMain::notice_until_ms = 0;

void UITabMain::create(lv_obj_t *tab) {
    lv_obj_set_style_bg_color(tab, UITheme::BG_BLACK, LV_PART_MAIN);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    // No theme padding - positions below are absolute within the 800x360 tab
    // area and include their own 10px margins
    lv_obj_set_style_pad_all(tab, 0, 0);

    // Layout, grouped by task:
    //   left panel  (x 10..450)   MOVE   - state, X position (tap to go to), jog, message
    //   right panel (x 470..790)  REPEAT - saved positions (+ Save)
    //   bottom bar  (y 290..350)  MACHINE - STOP | Home X | Zero X | Unlock (equal size)
    const int left_x = 10;
    const int left_w = 440;
    const int right_x = 470;
    const int right_w = 320;
    const int bar_y = 290;

    // Dividers between the areas
    static lv_point_precise_t vline_pts[] = {{0, 0}, {0, 262}};
    lv_obj_t *vline = lv_line_create(tab);
    lv_line_set_points(vline, vline_pts, 2);
    lv_obj_set_style_line_color(vline, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_line_width(vline, 1, 0);
    lv_obj_set_pos(vline, 460, 10);

    static lv_point_precise_t hline_pts[] = {{0, 0}, {780, 0}};
    lv_obj_t *hline = lv_line_create(tab);
    lv_line_set_points(hline, hline_pts, 2);
    lv_obj_set_style_line_color(hline, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_line_width(hline, 1, 0);
    lv_obj_set_pos(hline, 10, 280);

    // ========== LEFT: STATE + X POSITION ==========
    lv_obj_t *state_label = lv_label_create(tab);
    lv_label_set_text(state_label, "STATE");
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(state_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(state_label, left_x, 8);

    lbl_state = lv_label_create(tab);
    lv_label_set_text(lbl_state, "OFFLINE");
    lv_obj_set_style_text_font(lbl_state, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl_state, UITheme::STATE_ALARM, 0);
    lv_obj_set_pos(lbl_state, left_x, 26);
    fitStateLabel("OFFLINE");

    ind_limit_x = lv_obj_create(tab);
    lv_obj_set_size(ind_limit_x, 14, 14);
    lv_obj_set_pos(ind_limit_x, 196, 33);  // Left of the X label, like the Status tab
    lv_obj_set_style_radius(ind_limit_x, 7, 0);
    lv_obj_set_style_bg_color(ind_limit_x, UITheme::BG_BUTTON, 0);
    lv_obj_set_style_border_width(ind_limit_x, 1, 0);
    lv_obj_set_style_border_color(ind_limit_x, UITheme::BORDER_MEDIUM, 0);
    lv_obj_clear_flag(ind_limit_x, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wpos_x_label = lv_label_create(tab);
    lv_label_set_text(wpos_x_label, "X");
    lv_obj_set_style_text_font(wpos_x_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(wpos_x_label, UITheme::AXIS_X, 0);
    lv_obj_set_pos(wpos_x_label, 216, 21);

    lbl_wpos_x = lv_textarea_create(tab);
    lv_textarea_set_text(lbl_wpos_x, "----.---");
    lv_textarea_set_one_line(lbl_wpos_x, true);
    lv_textarea_set_max_length(lbl_wpos_x, 10);
    lv_obj_set_size(lbl_wpos_x, 200, 55);
    lv_obj_set_pos(lbl_wpos_x, left_x + left_w - 200, 12);
    lv_obj_clear_flag(lbl_wpos_x, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(lbl_wpos_x, &lv_font_montserrat_32, 0);
    // Explicit paddings and the same 2px border focused or not, so the content
    // area always fits the font - otherwise the textarea keeps scrolling to the
    // cursor while editing and the number bounces
    lv_obj_set_style_pad_top(lbl_wpos_x, 7, 0);
    lv_obj_set_style_pad_bottom(lbl_wpos_x, 0, 0);
    lv_obj_set_style_pad_left(lbl_wpos_x, 9, 0);
    lv_obj_set_style_text_align(lbl_wpos_x, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_color(lbl_wpos_x, UITheme::AXIS_X, 0);
    lv_obj_set_style_bg_color(lbl_wpos_x, UITheme::BG_BLACK, 0);
    lv_obj_set_style_border_width(lbl_wpos_x, 2, 0);
    lv_obj_set_style_border_color(lbl_wpos_x, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_border_color(lbl_wpos_x, UITheme::AXIS_X, LV_STATE_FOCUSED);
    lv_obj_set_user_data(lbl_wpos_x, (void*)"WX");
    lv_obj_add_event_cb(lbl_wpos_x, position_field_event_handler, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl_wpos_x, position_field_event_handler, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *goto_hint = lv_label_create(tab);
    lv_label_set_text(goto_hint, "Tap position to go to");
    lv_obj_set_style_text_font(goto_hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(goto_hint, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(goto_hint, left_x + left_w - 198, 70);

    // ========== LEFT: JOG ==========
    loadStepValues();

    lv_obj_t *jog_label = lv_label_create(tab);
    lv_label_set_text(jog_label, "JOG STEP (mm)");
    lv_obj_set_style_text_font(jog_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(jog_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(jog_label, left_x, 90);

    // Up to 5 step buttons spread across the panel width
    const int step_gap = 10;
    const int step_w = (left_w - (MAX_STEPS - 1) * step_gap) / MAX_STEPS;
    for (int i = 0; i < MAX_STEPS; i++) {
        step_buttons[i] = nullptr;
        if (i >= step_count) continue;
        lv_obj_t *btn = lv_button_create(tab);
        lv_obj_set_size(btn, step_w, 44);
        lv_obj_set_pos(btn, left_x + i * (step_w + step_gap), 112);
        lv_obj_t *lbl = lv_label_create(btn);
        char buf[16];
        snprintf(buf, sizeof(buf), "%g", step_values[i]);
        lv_label_set_text(lbl, buf);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, onStepClicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        step_buttons[i] = btn;
    }
    updateStepButtonStyles();

    const int jog_w = (left_w - step_gap) / 2;
    lv_obj_t *btn_minus = lv_button_create(tab);
    lv_obj_set_size(btn_minus, jog_w, 50);
    lv_obj_set_pos(btn_minus, left_x, 166);
    lv_obj_set_style_bg_color(btn_minus, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, LV_SYMBOL_LEFT "  X-");
    lv_obj_set_style_text_font(lbl_minus, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_minus);
    lv_obj_add_event_cb(btn_minus, onJogClicked, LV_EVENT_CLICKED, (void*)(intptr_t)-1);

    lv_obj_t *btn_plus = lv_button_create(tab);
    lv_obj_set_size(btn_plus, jog_w, 50);
    lv_obj_set_pos(btn_plus, left_x + jog_w + step_gap, 166);
    lv_obj_set_style_bg_color(btn_plus, UITheme::AXIS_X, LV_PART_MAIN);
    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, "X+  " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl_plus);
    lv_obj_add_event_cb(btn_plus, onJogClicked, LV_EVENT_CLICKED, (void*)(intptr_t)1);

    // ========== LEFT: MESSAGE (same box styling as the Status tab) ==========
    lbl_message = lv_label_create(tab);
    lv_label_set_text(lbl_message, "No messages.");
    lv_obj_set_size(lbl_message, left_w, 46);
    lv_label_set_long_mode(lbl_message, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(lbl_message, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_message, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_bg_color(lbl_message, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_bg_opa(lbl_message, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lbl_message, 1, 0);
    lv_obj_set_style_border_color(lbl_message, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_radius(lbl_message, 5, 0);
    lv_obj_set_style_pad_all(lbl_message, 10, 0);
    lv_obj_set_pos(lbl_message, left_x, 226);

    // ========== RIGHT: SAVED POSITIONS ==========
    lv_obj_t *saved_label = lv_label_create(tab);
    lv_label_set_text(saved_label, "SAVED POSITIONS");
    lv_obj_set_style_text_font(saved_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(saved_label, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(saved_label, right_x, 18);

    lv_obj_t *btn_save = lv_button_create(tab);
    lv_obj_set_size(btn_save, 110, 40);
    lv_obj_set_pos(btn_save, right_x + right_w - 110, 8);
    lv_obj_set_style_bg_color(btn_save, UITheme::BTN_PLAY, LV_PART_MAIN);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, LV_SYMBOL_PLUS " Save");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save, onSaveClicked, LV_EVENT_CLICKED, nullptr);

    // 4 rows x 2 columns of saved position buttons
    const int slot_gap = 10;
    const int slot_w = (right_w - slot_gap) / 2;
    const int slot_h = 44;
    for (int i = 0; i < MAX_SAVED; i++) {
        lv_obj_t *btn = lv_button_create(tab);
        lv_obj_set_size(btn, slot_w, slot_h);
        lv_obj_set_pos(btn, right_x + (i % 2) * (slot_w + slot_gap), 58 + (i / 2) * (slot_h + slot_gap));
        lv_obj_set_style_bg_color(btn, UITheme::BG_BUTTON, LV_PART_MAIN);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lbl, UITheme::AXIS_X, 0);
        lv_obj_center(lbl);
        // SHORT_CLICKED (not CLICKED) so a long press to delete doesn't also go there
        lv_obj_add_event_cb(btn, onSavedClicked, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(btn, onSavedLongPressed, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
        saved_buttons[i] = btn;
    }

    lbl_saved_hint = lv_label_create(tab);
    lv_label_set_text(lbl_saved_hint, "Jog or go to a position, then\ntap Save to add it here.\n\n"
                                      "Tap a saved position to go\nthere, long-press to delete it.");
    lv_obj_set_style_text_font(lbl_saved_hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_saved_hint, UITheme::TEXT_DISABLED, 0);
    lv_obj_set_pos(lbl_saved_hint, right_x, 70);

    loadSavedPositions();
    refreshSavedButtons();

    // ========== BOTTOM: ACTION BAR (4 equal buttons) ==========
    const int bar_gap = 10;
    const int bar_w = (780 - 3 * bar_gap) / 4;
    const int bar_h = 60;
    struct ActionButton {
        const char *text;
        lv_color_t color;
        lv_event_cb_t cb;
        lv_obj_t **store;
    } actions[] = {
        {LV_SYMBOL_STOP " STOP", UITheme::BTN_ESTOP, onQuickStopClicked, nullptr},
        {LV_SYMBOL_HOME " Home X", UITheme::AXIS_X, onHomeXClicked, &btn_home_x},
        {LV_SYMBOL_GPS " Zero X", UITheme::AXIS_X, onZeroXClicked, nullptr},
        {LV_SYMBOL_OK " Unlock", UITheme::BTN_UNLOCK, onUnlockClicked, nullptr},
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_button_create(tab);
        lv_obj_set_size(btn, bar_w, bar_h);
        lv_obj_set_pos(btn, 10 + i * (bar_w + bar_gap), bar_y);
        lv_obj_set_style_bg_color(btn, actions[i].color, LV_PART_MAIN);
        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, actions[i].text);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, actions[i].cb, LV_EVENT_CLICKED, nullptr);
        if (actions[i].store) *actions[i].store = btn;
    }
}

// ========== JOGGING ==========

void UITabMain::loadStepValues() {
    // Same comma-separated list the Control > Jog tab uses for XY
    char buffer[64];
    strncpy(buffer, UITabSettingsJog::getXYSteps(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    step_count = 0;
    char *token = strtok(buffer, ",");
    while (token != nullptr && step_count < MAX_STEPS) {
        step_values[step_count++] = atof(token);
        token = strtok(nullptr, ",");
    }
    if (step_count == 0) {
        step_values[0] = 1.0f;
        step_count = 1;
    }

    // Start on the configured default step (closest match)
    float default_step = UITabSettingsJog::getDefaultXYStep();
    step_index = 0;
    for (int i = 1; i < step_count; i++) {
        if (fabsf(step_values[i] - default_step) < fabsf(step_values[step_index] - default_step)) {
            step_index = i;
        }
    }
}

void UITabMain::updateStepButtonStyles() {
    for (int i = 0; i < step_count; i++) {
        if (!step_buttons[i]) continue;
        lv_obj_set_style_bg_color(step_buttons[i],
            i == step_index ? UITheme::ACCENT_PRIMARY : UITheme::BG_BUTTON, LV_PART_MAIN);
    }
}

void UITabMain::onStepClicked(lv_event_t *e) {
    step_index = (int)(intptr_t)lv_event_get_user_data(e);
    updateStepButtonStyles();
}

void UITabMain::onJogClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) return;
    int direction = (int)(intptr_t)lv_event_get_user_data(e);
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "$J=G91 X%.3f F%d\n",
             direction * step_values[step_index], UITabSettingsJog::getDefaultXYFeed());
    Serial.printf("[Main] Jog: %s", cmd);
    FluidNCClient::sendCommand(cmd);
}

// ========== SAVED POSITIONS ==========

void UITabMain::loadSavedPositions() {
    saved_count = 0;
    int machine = MachineConfigManager::getSelectedMachineIndex();
    if (machine < 0) return;

    char key[16];
    snprintf(key, sizeof(key), "m%d_qpos", machine);
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true)) return;
    String list = prefs.getString(key, "");
    prefs.end();

    int start = 0;
    while (start < (int)list.length() && saved_count < MAX_SAVED) {
        int comma = list.indexOf(',', start);
        if (comma < 0) comma = list.length();
        String item = list.substring(start, comma);
        if (item.length() > 0) saved_positions[saved_count++] = item.toFloat();
        start = comma + 1;
    }
}

void UITabMain::storeSavedPositions() {
    int machine = MachineConfigManager::getSelectedMachineIndex();
    if (machine < 0) return;

    String list;
    for (int i = 0; i < saved_count; i++) {
        if (i > 0) list += ",";
        list += String(saved_positions[i], 3);
    }

    char key[16];
    snprintf(key, sizeof(key), "m%d_qpos", machine);
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, false)) return;
    prefs.putString(key, list);
    prefs.end();
}

void UITabMain::refreshSavedButtons() {
    for (int i = 0; i < MAX_SAVED; i++) {
        if (!saved_buttons[i]) continue;
        if (i < saved_count) {
            char buf[20];
            snprintf(buf, sizeof(buf), "%.3f", saved_positions[i]);
            lv_label_set_text(lv_obj_get_child(saved_buttons[i], 0), buf);
            lv_obj_clear_flag(saved_buttons[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(saved_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (lbl_saved_hint) {
        if (saved_count == 0) lv_obj_clear_flag(lbl_saved_hint, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(lbl_saved_hint, LV_OBJ_FLAG_HIDDEN);
    }
    updateSavedHighlight();
}

// White border on the saved position the machine is currently at - same
// indicator style as the Home buttons on the Control > Actions tab
void UITabMain::updateSavedHighlight() {
    for (int i = 0; i < MAX_SAVED; i++) {
        if (!saved_buttons[i]) continue;
        bool here = i < saved_count && last_wpos_x > -9999.0f &&
                    fabsf(saved_positions[i] - last_wpos_x) < 0.0005f;
        lv_obj_set_style_border_width(saved_buttons[i], here ? 3 : 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(saved_buttons[i], lv_color_white(), LV_PART_MAIN);
    }
}

void UITabMain::fitStateLabel(const char *state) {
    if (!lbl_state) return;
    // Largest font that fits the column left of the X position, keeping the
    // text vertically centred on the 32px baseline row
    const lv_font_t *fonts[] = {&lv_font_montserrat_32, &lv_font_montserrat_24, &lv_font_montserrat_20};
    const lv_font_t *chosen = fonts[2];
    for (const lv_font_t *font : fonts) {
        lv_point_t size;
        lv_text_get_size(&size, state, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x <= STATE_MAX_WIDTH) {
            chosen = font;
            break;
        }
    }
    lv_obj_set_style_text_font(lbl_state, chosen, 0);
    lv_obj_set_y(lbl_state, 26 + (lv_font_get_line_height(&lv_font_montserrat_32) - lv_font_get_line_height(chosen)) / 2);
}

void UITabMain::goToWorkX(float x) {
    if (!FluidNCClient::isConnected()) return;
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "$J=X%.3f F%d\n", x, UITabSettingsJog::getDefaultXYFeed());
    Serial.printf("[Main] Go to saved position: %s", cmd);
    FluidNCClient::sendCommand(cmd);
}

void UITabMain::onSaveClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) return;
    float x = FluidNCClient::getStatus().wpos_x;
    if (x <= -9999.0f) return;

    // Ignore a position that's already saved (to the displayed 0.001mm)
    for (int i = 0; i < saved_count; i++) {
        if (fabsf(saved_positions[i] - x) < 0.0005f) {
            showNotice("Position already saved");
            return;
        }
    }
    if (saved_count >= MAX_SAVED) {
        showNotice("Saved positions full - long-press one to delete it");
        return;
    }

    // Keep the list sorted so it reads like a cut list
    int insert_at = saved_count;
    while (insert_at > 0 && saved_positions[insert_at - 1] > x) {
        saved_positions[insert_at] = saved_positions[insert_at - 1];
        insert_at--;
    }
    saved_positions[insert_at] = x;
    saved_count++;

    storeSavedPositions();
    refreshSavedButtons();
}

void UITabMain::onSavedClicked(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (index < saved_count) goToWorkX(saved_positions[index]);
}

void UITabMain::onSavedLongPressed(lv_event_t *e) {
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    if (index < saved_count) showDeleteDialog(index);
}

void UITabMain::showDeleteDialog(int index) {
    pending_delete_index = index;

    lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
    lv_obj_set_size(backdrop, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_set_style_border_width(backdrop, 0, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dialog = lv_obj_create(backdrop);
    lv_obj_set_size(dialog, 420, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_color(dialog, UITheme::BORDER_MEDIUM, 0);
    lv_obj_set_style_border_width(dialog, 2, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *msg = lv_label_create(dialog);
    char buf[64];
    snprintf(buf, sizeof(buf), "Delete saved position\nX %.3f?", saved_positions[index]);
    lv_label_set_text(msg, buf);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(msg, UITheme::TEXT_LIGHT, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *btn_delete = lv_button_create(dialog);
    lv_obj_set_size(btn_delete, 170, 50);
    lv_obj_align(btn_delete, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_delete, UITheme::BTN_ESTOP, LV_PART_MAIN);
    lv_obj_t *lbl_delete = lv_label_create(btn_delete);
    lv_label_set_text(lbl_delete, LV_SYMBOL_TRASH " Delete");
    lv_obj_set_style_text_font(lbl_delete, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_delete);
    lv_obj_add_event_cb(btn_delete, [](lv_event_t *e) {
        int i = pending_delete_index;
        if (i >= 0 && i < saved_count) {
            for (int j = i; j < saved_count - 1; j++) saved_positions[j] = saved_positions[j + 1];
            saved_count--;
            storeSavedPositions();
            refreshSavedButtons();
        }
        pending_delete_index = -1;
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, backdrop);

    lv_obj_t *btn_cancel = lv_button_create(dialog);
    lv_obj_set_size(btn_cancel, 170, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_cancel, UITheme::BG_BUTTON, LV_PART_MAIN);
    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE " Cancel");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_cancel);
    lv_obj_add_event_cb(btn_cancel, [](lv_event_t *e) {
        pending_delete_index = -1;
        lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, backdrop);
}

// ========== UPDATE METHODS ==========

// Show a message from this tab (e.g. "Position already saved") and keep it up
// for NOTICE_HOLD_MS - main.cpp pushes FluidNC's last message into the box every
// 250ms, which would otherwise replace it before it could be read.
void UITabMain::showNotice(const char *message) {
    if (!lbl_message) return;
    lv_label_set_text(lbl_message, message);
    lv_obj_set_style_text_color(lbl_message, UITheme::UI_WARNING, 0);
    notice_until_ms = millis() + NOTICE_HOLD_MS;
}

void UITabMain::updateMessage(const char *message) {
    if (!lbl_message) return;
    if ((int32_t)(notice_until_ms - millis()) > 0) return;  // Notice still showing
    lv_label_set_text(lbl_message, message);

    if (strncmp(message, "ERROR", 5) == 0 || strstr(message, "error") != nullptr) {
        lv_obj_set_style_text_color(lbl_message, UITheme::STATE_ALARM, 0);
    } else if (strncmp(message, "WARN", 4) == 0 || strstr(message, "warning") != nullptr) {
        lv_obj_set_style_text_color(lbl_message, UITheme::UI_WARNING, 0);
    } else if (strncmp(message, "OK", 2) == 0 || strstr(message, "success") != nullptr) {
        lv_obj_set_style_text_color(lbl_message, UITheme::UI_SUCCESS, 0);
    } else {
        lv_obj_set_style_text_color(lbl_message, UITheme::UI_INFO, 0);
    }
}

void UITabMain::updateState(const char *state) {
    if (!lbl_state) return;
    if (strcmp(state, last_state) == 0) return;

    lv_label_set_text(lbl_state, state);
    fitStateLabel(state);
    strncpy(last_state, state, sizeof(last_state) - 1);
    last_state[sizeof(last_state) - 1] = '\0';

    if (strcmp(state, "IDLE") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_IDLE, 0);
    } else if (strcmp(state, "RUN") == 0 || strcmp(state, "JOG") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_RUN, 0);
    } else if (strcmp(state, "ALARM") == 0 || strcmp(state, "OFFLINE") == 0) {
        lv_obj_set_style_text_color(lbl_state, UITheme::STATE_ALARM, 0);
    } else {
        lv_obj_set_style_text_color(lbl_state, UITheme::UI_WARNING, 0);
    }
}

void UITabMain::updateWorkPosition(float x, float y, float z, float a) {
    if (!lbl_wpos_x || x == last_wpos_x) return;

    char buf[20];
    if (x <= -9999.0f) {
        lv_textarea_set_text(lbl_wpos_x, "----.---");
    } else {
        snprintf(buf, sizeof(buf), "%.3f", x);
        lv_textarea_set_text(lbl_wpos_x, buf);
    }
    last_wpos_x = x;
    updateSavedHighlight();
}

void UITabMain::updateMachinePosition(float x, float y, float z, float a) {
    if (!lbl_mpos_x || x == last_mpos_x) return;

    char buf[20];
    if (x <= -9999.0f) {
        lv_textarea_set_text(lbl_mpos_x, "----.---");
    } else {
        snprintf(buf, sizeof(buf), "%.3f", x);
        lv_textarea_set_text(lbl_mpos_x, buf);
    }
    last_mpos_x = x;
}

void UITabMain::updateLimitSwitches(bool x, bool y, bool z, bool a) {
    if (!ind_limit_x) return;

    uint32_t now = millis();
    if (x) last_limit_trigger_x_ms = now;
    bool vis_x = x || (now - last_limit_trigger_x_ms < LIMIT_SWITCH_HOLD_MS);

    lv_obj_set_style_bg_color(ind_limit_x, vis_x ? UITheme::STATE_ALARM : UITheme::BG_BUTTON, 0);
}

// ========== POSITION EDITING (Go To), same pattern as UITabStatus ==========

void UITabMain::position_field_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *textarea = (lv_obj_t*)lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) {
        const FluidNCStatus& status = FluidNCClient::getStatus();
        if (status.state != STATE_IDLE) {
            lv_obj_clear_state(textarea, LV_STATE_FOCUSED);
            return;
        }

        active_textarea = textarea;
        strncpy(original_value, lv_textarea_get_text(textarea), sizeof(original_value) - 1);
        original_value[sizeof(original_value) - 1] = '\0';

        if (!keyboard) {
            keyboard = lv_obj_create(lv_scr_act());
            lv_obj_set_size(keyboard, 325, 340);
            lv_obj_set_pos(keyboard, 465, 70);
            lv_obj_set_style_bg_color(keyboard, UITheme::BG_DARK, 0);
            lv_obj_set_style_border_color(keyboard, UITheme::BORDER_MEDIUM, 0);
            lv_obj_set_style_border_width(keyboard, 2, 0);
            lv_obj_set_style_pad_all(keyboard, 10, 0);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            const char* num_labels[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", ".", "0", "-"};
            int btn_width = 97;
            int btn_height = 48;
            int gap = 5;

            for (int i = 0; i < 12; i++) {
                int row = i / 3;
                int col = i % 3;

                lv_obj_t *btn = lv_button_create(keyboard);
                lv_obj_set_size(btn, btn_width, btn_height);
                lv_obj_set_pos(btn, col * (btn_width + gap), row * (btn_height + gap));
                lv_obj_set_style_bg_color(btn, UITheme::BG_MEDIUM, 0);
                lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);

                lv_obj_t *label = lv_label_create(btn);
                lv_label_set_text(label, num_labels[i]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
                lv_obj_center(label);

                lv_obj_set_user_data(btn, (void*)num_labels[i]);
                lv_obj_add_event_cb(btn, [](lv_event_t *e) {
                    if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                        lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
                        const char* ch = (const char*)lv_obj_get_user_data(target);
                        lv_textarea_add_text(active_textarea, ch);
                    }
                }, LV_EVENT_CLICKED, NULL);
            }

            lv_obj_t *btn_clear = lv_button_create(keyboard);
            lv_obj_set_size(btn_clear, (btn_width * 2) + gap, btn_height);
            lv_obj_set_pos(btn_clear, 0, (btn_height * 4) + (gap * 4));
            lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xFF6600), 0);
            lv_obj_clear_flag(btn_clear, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_clear = lv_label_create(btn_clear);
            lv_label_set_text(lbl_clear, "CLEAR");
            lv_obj_set_style_text_font(lbl_clear, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_clear);

            lv_obj_add_event_cb(btn_clear, [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                    lv_textarea_set_text(active_textarea, "");
                }
            }, LV_EVENT_CLICKED, NULL);

            lv_obj_t *btn_back = lv_button_create(keyboard);
            lv_obj_set_size(btn_back, btn_width, btn_height);
            lv_obj_set_pos(btn_back, (btn_width * 2) + (gap * 2), (btn_height * 4) + (gap * 4));
            lv_obj_set_style_bg_color(btn_back, UITheme::BG_MEDIUM, 0);
            lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_back = lv_label_create(btn_back);
            lv_label_set_text(lbl_back, LV_SYMBOL_BACKSPACE);
            lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_24, 0);
            lv_obj_center(lbl_back);

            lv_obj_add_event_cb(btn_back, [](lv_event_t *e) {
                if (lv_event_get_code(e) == LV_EVENT_CLICKED && active_textarea) {
                    lv_textarea_delete_char(active_textarea);
                }
            }, LV_EVENT_CLICKED, NULL);

            lv_obj_t *btn_ok = lv_button_create(keyboard);
            lv_obj_set_size(btn_ok, (((btn_width * 3) + (gap * 2)) / 2) - 3, btn_height);
            lv_obj_set_pos(btn_ok, 0, (btn_height * 5) + (gap * 5));
            lv_obj_set_style_bg_color(btn_ok, UITheme::BTN_PLAY, 0);
            lv_obj_clear_flag(btn_ok, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_ok = lv_label_create(btn_ok);
            lv_label_set_text(lbl_ok, "OK");
            lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_ok);

            lv_obj_add_event_cb(btn_ok, keyboard_event_handler, LV_EVENT_CLICKED, NULL);
            lv_obj_set_user_data(btn_ok, (void*)"ok");

            lv_obj_t *btn_cancel = lv_button_create(keyboard);
            lv_obj_set_size(btn_cancel, (((btn_width * 3) + (gap * 2)) / 2) - 3, btn_height);
            lv_obj_set_pos(btn_cancel, (((btn_width * 3) + (gap * 2)) / 2) + 3, (btn_height * 5) + (gap * 5));
            lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x555555), 0);
            lv_obj_clear_flag(btn_cancel, LV_OBJ_FLAG_CLICK_FOCUSABLE);

            lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
            lv_label_set_text(lbl_cancel, "CANCEL");
            lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
            lv_obj_center(lbl_cancel);

            lv_obj_add_event_cb(btn_cancel, keyboard_event_handler, LV_EVENT_CLICKED, NULL);
            lv_obj_set_user_data(btn_cancel, (void*)"cancel");
        }

        lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }

    if (code == LV_EVENT_DEFOCUSED) {
        if (keyboard && !lv_obj_has_flag(keyboard, LV_OBJ_FLAG_HIDDEN)) {
            // lbl_mpos_x isn't created on this tab (machine position is on the status bar)
            bool switching_fields = ((lbl_wpos_x && lv_obj_has_state(lbl_wpos_x, LV_STATE_FOCUSED)) ||
                                     (lbl_mpos_x && lv_obj_has_state(lbl_mpos_x, LV_STATE_FOCUSED)));

            if (!switching_fields) {
                if (active_textarea) {
                    lv_textarea_set_text(active_textarea, original_value);
                }
                lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
                active_textarea = nullptr;
            }
        }
    }
}

void UITabMain::showValidationError(const char* message) {
    lv_obj_t *dialog = lv_obj_create(lv_screen_active());
    lv_obj_set_size(dialog, 400, 200);
    lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog, UITheme::BG_DARK, 0);
    lv_obj_set_style_border_color(dialog, UITheme::STATE_ALARM, 0);
    lv_obj_set_style_border_width(dialog, 3, 0);

    lv_obj_t *title = lv_label_create(dialog);
    lv_label_set_text(title, "VALIDATION ERROR");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::STATE_ALARM, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *msg = lv_label_create(dialog);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(msg, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn = lv_btn_create(dialog);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(btn, UITheme::BTN_CONNECT, 0);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "OK");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_18, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(btn, [](lv_event_t *e) {
        lv_obj_t *dialog = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_delete(dialog);
    }, LV_EVENT_CLICKED, dialog);
}

void UITabMain::keyboard_event_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t*)lv_event_get_target(e);

    if (code != LV_EVENT_CLICKED) return;

    const char* action = (const char*)lv_obj_get_user_data(btn);

    if (strcmp(action, "ok") == 0) {
        if (active_textarea) {
            const char *axis_id = (const char*)lv_obj_get_user_data(active_textarea);
            const char *value_str = lv_textarea_get_text(active_textarea);

            char *endptr;
            float value = strtof(value_str, &endptr);
            if (endptr == value_str || *endptr != '\0') {
                showValidationError("Invalid number entered");
                return;
            }

            char command[64];
            int feed_rate = UITabSettingsJog::getDefaultXYFeed();

            if (axis_id[0] == 'W') {
                snprintf(command, sizeof(command), "$J=X%.3f F%d\n", value, feed_rate);
                Serial.printf("[Main] Jogging to work X position: %s", command);
            } else if (axis_id[0] == 'M') {
                snprintf(command, sizeof(command), "$J=G53 X%.3f F%d\n", value, feed_rate);
                Serial.printf("[Main] Jogging to machine X position: %s", command);
            }

            FluidNCClient::sendCommand(command);
        }

        if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        active_textarea = nullptr;

    } else if (strcmp(action, "cancel") == 0) {
        if (active_textarea) {
            lv_textarea_set_text(active_textarea, original_value);
            FluidNCClient::sendCommand("?");
        }

        if (keyboard) lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        active_textarea = nullptr;
    }
}

// ========== ACTION BUTTON HANDLERS ==========

void UITabMain::onHomeXClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }
    Serial.println("[Main] Sending Home X command ($HX)");
    FluidNCClient::sendCommand("$HX\n");
}

void UITabMain::onZeroXClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }

    if (WCSConfig::isCurrentWCSLocked()) {
        const FluidNCStatus& status = FluidNCClient::getStatus();
        char wcs_name[32];
        WCSConfig::getCurrentWCSName(wcs_name, sizeof(wcs_name));

        Serial.printf("[Main] WCS %s is locked, showing confirmation\n", status.modal_wcs);
        UICommon::showWCSLockDialog(status.modal_wcs, wcs_name, [](lv_event_t *e) {
            Serial.println("[Main] Confirmed Zero X on locked WCS");
            FluidNCClient::sendCommand("G10 L20 P0 X0\n");
        });
        return;
    }

    Serial.println("[Main] Sending Zero X command (G10 L20 P0 X0)");
    FluidNCClient::sendCommand("G10 L20 P0 X0\n");
}

void UITabMain::onUnlockClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }
    Serial.println("[Main] Sending Unlock command ($X)");
    FluidNCClient::sendCommand("$X\n");
}

void UITabMain::onQuickStopClicked(lv_event_t *e) {
    if (!FluidNCClient::isConnected()) {
        Serial.println("[Main] Not connected to FluidNC");
        return;
    }
    Serial.println("[Main] Sending Quick Stop (feed hold !)");
    FluidNCClient::sendCommand("!");
}
