#include "ui/ui_machine_select.h"
#include "ui/ui_common.h"
#include "ui/ui_tabs.h"
#include "ui/ui_theme.h"
#include "config.h"
#include <Preferences.h>

lv_obj_t *UIMachineSelect::screen = nullptr;
lv_display_t *UIMachineSelect::display = nullptr;

void UIMachineSelect::show(lv_display_t *disp) {
    display = disp;
    Serial.println("UIMachineSelect: Creating machine selection screen");
    
    // Create screen
    screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(screen, UITheme::BG_DARK, 0);
    
    // Title
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Select Machine");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);
    
    // Subtitle
    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Choose the CNC machine to control");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(subtitle, UITheme::TEXT_MEDIUM, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 65);
    
    // Machine button container
    lv_obj_t *btn_container = lv_obj_create(screen);
    lv_obj_set_size(btn_container, 700, 380);
    lv_obj_set_style_bg_color(btn_container, UITheme::BG_MEDIUM, 0);
    lv_obj_set_style_border_width(btn_container, 0, 0);
    lv_obj_set_style_pad_all(btn_container, 20, 0);
    lv_obj_align(btn_container, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Machine 1: V1E LowRider 3 (Wireless)
    lv_obj_t *btn_lowrider = lv_btn_create(btn_container);
    lv_obj_set_size(btn_lowrider, 660, 80);
    lv_obj_set_style_bg_color(btn_lowrider, UITheme::ACCENT_PRIMARY, 0);
    lv_obj_set_style_bg_color(btn_lowrider, lv_color_lighten(UITheme::ACCENT_PRIMARY, 30), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_lowrider, onMachineSelected, LV_EVENT_CLICKED, (void*)"V1E LowRider 3");
    
    lv_obj_t *label_lowrider = lv_label_create(btn_lowrider);
    lv_label_set_text(label_lowrider, LV_SYMBOL_WIFI " V1E LowRider 3");
    lv_obj_set_style_text_font(label_lowrider, &lv_font_montserrat_22, 0);
    lv_obj_center(label_lowrider);
    
    // Machine 2: Pen Plotter (Wireless)
    lv_obj_t *btn_plotter = lv_btn_create(btn_container);
    lv_obj_set_size(btn_plotter, 660, 80);
    lv_obj_set_style_bg_color(btn_plotter, UITheme::ACCENT_SECONDARY, 0);
    lv_obj_set_style_bg_color(btn_plotter, lv_color_lighten(UITheme::ACCENT_SECONDARY, 30), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_plotter, onMachineSelected, LV_EVENT_CLICKED, (void*)"Pen Plotter");
    
    lv_obj_t *label_plotter = lv_label_create(btn_plotter);
    lv_label_set_text(label_plotter, LV_SYMBOL_WIFI " Pen Plotter");
    lv_obj_set_style_text_font(label_plotter, &lv_font_montserrat_22, 0);
    lv_obj_center(label_plotter);
    
    // Machine 3: Yeagbot (Wireless)
    lv_obj_t *btn_yeagbot = lv_btn_create(btn_container);
    lv_obj_set_size(btn_yeagbot, 660, 80);
    lv_obj_set_style_bg_color(btn_yeagbot, UITheme::STATE_RUN, 0);
    lv_obj_set_style_bg_color(btn_yeagbot, lv_color_lighten(UITheme::STATE_RUN, 30), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_yeagbot, onMachineSelected, LV_EVENT_CLICKED, (void*)"Yeagbot");
    
    lv_obj_t *label_yeagbot = lv_label_create(btn_yeagbot);
    lv_label_set_text(label_yeagbot, LV_SYMBOL_WIFI " Yeagbot");
    lv_obj_set_style_text_font(label_yeagbot, &lv_font_montserrat_22, 0);
    lv_obj_center(label_yeagbot);
    
    // Machine 4: Test Wired Machine (Wired)
    lv_obj_t *btn_wired = lv_btn_create(btn_container);
    lv_obj_set_size(btn_wired, 660, 80);
    lv_obj_set_style_bg_color(btn_wired, UITheme::UI_WARNING, 0);
    lv_obj_set_style_bg_color(btn_wired, lv_color_lighten(UITheme::UI_WARNING, 30), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_wired, onMachineSelected, LV_EVENT_CLICKED, (void*)"Test Wired Machine");
    
    lv_obj_t *label_wired = lv_label_create(btn_wired);
    lv_label_set_text(label_wired, LV_SYMBOL_USB " Test Wired Machine");
    lv_obj_set_style_text_font(label_wired, &lv_font_montserrat_22, 0);
    lv_obj_center(label_wired);
    
    // Load screen
    lv_scr_load(screen);
    
    Serial.println("UIMachineSelect: Machine selection screen displayed");
}

void UIMachineSelect::hide() {
    if (screen) {
        lv_obj_del(screen);
        screen = nullptr;
    }
}

void UIMachineSelect::onMachineSelected(lv_event_t *e) {
    const char *machine_name = (const char*)lv_event_get_user_data(e);
    
    Serial.printf("UIMachineSelect: Machine selected: %s\n", machine_name);
    
    // Save selection to preferences
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString("machine", machine_name);
    prefs.end();
    
    // Initialize main UI with saved display reference
    UICommon::init(display);
    
    // Create a new screen for the main UI
    lv_obj_t *main_screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(main_screen, UITheme::BG_DARKER, LV_PART_MAIN);
    
    // Load the new screen BEFORE deleting the old one
    lv_scr_load(main_screen);
    
    // Now safe to delete the machine selection screen
    hide();
    
    // Create status bar using UICommon module
    UICommon::createStatusBar();
    
    // Create all tabs using UITabs module
    UITabs::createTabs();
    
    Serial.println("UIMachineSelect: Main UI initialized");
}
