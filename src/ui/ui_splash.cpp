#include "ui/ui_splash.h"
#include "fluidnc_logo.h"

void UISplash::show(lv_display_t *disp) {
    lv_obj_t *splash = lv_obj_create(lv_screen_active());
    lv_obj_set_size(splash, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(splash, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(splash, 0, 0);
    lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
    
    // FluidNC Logo Image (365x136 pixels)
    lv_obj_t *logo_img = lv_img_create(splash);
    lv_img_set_src(logo_img, &fluidnc_logo);
    lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, -40);
    
    // Version info
    lv_obj_t *version = lv_label_create(splash);
    lv_label_set_text(version, "FluidTouch v1.0");
    lv_obj_set_style_text_font(version, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(version, lv_color_hex(0x00BFFF), 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 40);
    
    // Loading text
    lv_obj_t *loading = lv_label_create(splash);
    lv_label_set_text(loading, "Initializing...");
    lv_obj_set_style_text_font(loading, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(loading, lv_color_hex(0x00FF00), 0);
    lv_obj_align(loading, LV_ALIGN_BOTTOM_MID, 0, -40);
    
    // Force LVGL to render the splash screen
    lv_refr_now(disp);
    
    // Show splash for configured duration
    delay(SPLASH_DURATION_MS);
    
    // Delete splash screen
    lv_obj_del(splash);
}
