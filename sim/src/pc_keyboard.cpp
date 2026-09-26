// PC keyboard -> on-screen keyboard bridge.
//
// When one of the firmware's lv_keyboard widgets is visible, typing on the
// PC keyboard inserts text into the textarea that keyboard targets, exactly
// as tapping its keys would. Enter = OK (LV_EVENT_READY), Esc = close
// (LV_EVENT_CANCEL), Backspace deletes. Nothing else about input changes,
// so on-screen keyboard behaviour stays identical to the device.

#include <Arduino.h>
#include <lvgl.h>
#include <SDL2/SDL.h>

#include <deque>
#include <string>

namespace sim {

namespace {

enum class KeyAction { Text, Enter, Escape, Backspace, Left, Right };
struct KeyEvent {
    KeyAction action;
    std::string text;
};
std::deque<KeyEvent> g_queue;

lv_obj_t *findKeyboard(lv_obj_t *obj) {
    if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(obj, &lv_keyboard_class)) return obj;
    uint32_t n = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < n; i++) {
        if (lv_obj_t *kb = findKeyboard(lv_obj_get_child(obj, i))) return kb;
    }
    return nullptr;
}

lv_obj_t *activeKeyboard() {
    lv_obj_t *kb = findKeyboard(lv_layer_top());
    if (!kb) kb = findKeyboard(lv_screen_active());
    return kb;
}

// Mirrors lv_keyboard_def_event_cb: send to the keyboard, then its textarea
void sendToKeyboardAndTextarea(lv_obj_t *kb, lv_event_code_t code) {
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    if (lv_obj_send_event(kb, code, nullptr) != LV_RESULT_OK) return;
    if (ta) lv_obj_send_event(ta, code, nullptr);
}

} // namespace

void pcKeyboardQueueText(const std::string &text) { g_queue.push_back({KeyAction::Text, text}); }

bool pcKeyboardQueueKey(const std::string &name) {
    if (name == "enter") g_queue.push_back({KeyAction::Enter, ""});
    else if (name == "esc") g_queue.push_back({KeyAction::Escape, ""});
    else if (name == "backspace") g_queue.push_back({KeyAction::Backspace, ""});
    else if (name == "left") g_queue.push_back({KeyAction::Left, ""});
    else if (name == "right") g_queue.push_back({KeyAction::Right, ""});
    else return false;
    return true;
}

// Called from an SDL event watch (don't touch LVGL here - just queue)
void pcKeyboardOnSdlEvent(const SDL_Event *e) {
    if (e->type == SDL_TEXTINPUT) {
        pcKeyboardQueueText(e->text.text);
    } else if (e->type == SDL_KEYDOWN) {
        switch (e->key.keysym.sym) {
            case SDLK_RETURN: case SDLK_KP_ENTER: pcKeyboardQueueKey("enter"); break;
            case SDLK_ESCAPE: pcKeyboardQueueKey("esc"); break;
            case SDLK_BACKSPACE: pcKeyboardQueueKey("backspace"); break;
            case SDLK_LEFT: pcKeyboardQueueKey("left"); break;
            case SDLK_RIGHT: pcKeyboardQueueKey("right"); break;
            default: break;
        }
    }
}

// Called from the main loop
void pcKeyboardProcess() {
    while (!g_queue.empty()) {
        KeyEvent ev = g_queue.front();
        g_queue.pop_front();
        lv_obj_t *kb = activeKeyboard();
        if (!kb) continue;  // no on-screen keyboard open - ignore typing
        lv_obj_t *ta = lv_keyboard_get_textarea(kb);
        switch (ev.action) {
            case KeyAction::Text: if (ta) lv_textarea_add_text(ta, ev.text.c_str()); break;
            case KeyAction::Backspace: if (ta) lv_textarea_delete_char(ta); break;
            case KeyAction::Left: if (ta) lv_textarea_cursor_left(ta); break;
            case KeyAction::Right: if (ta) lv_textarea_cursor_right(ta); break;
            case KeyAction::Enter: sendToKeyboardAndTextarea(kb, LV_EVENT_READY); break;
            case KeyAction::Escape: sendToKeyboardAndTextarea(kb, LV_EVENT_CANCEL); break;
        }
    }
}

} // namespace sim
