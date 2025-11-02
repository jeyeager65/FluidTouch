#include "ui/ui_splash.h"
#include "ui/ui_theme.h"
#include "ui/images/fluidnc_logo.h"
#include "config.h"

void UISplash::show(lv_display_t *disp) {
    lv_obj_t *splash = lv_obj_create(lv_screen_active());
    lv_obj_set_size(splash, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(splash, UITheme::BG_DARKER, 0);
    lv_obj_set_style_border_width(splash, 0, 0);
    lv_obj_clear_flag(splash, LV_OBJ_FLAG_SCROLLABLE);
    
    // FluidNC Logo Image (365x136 pixels)
    lv_obj_t *logo_img = lv_img_create(splash);
    lv_img_set_src(logo_img, &fluidnc_logo);
    lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, -70);
    
    // Product name
    lv_obj_t *product_name = lv_label_create(splash);
    lv_label_set_text(product_name, "FluidTouch");
    lv_obj_set_style_text_font(product_name, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(product_name, UITheme::UI_INFO, 0);
    lv_obj_align(product_name, LV_ALIGN_CENTER, 0, 20);
    
    // Tagline (below product name)
    lv_obj_t *tagline = lv_label_create(splash);
    lv_label_set_text(tagline, "CNC Touch Controller for FluidNC");
    lv_obj_set_style_text_font(tagline, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(tagline, UITheme::TEXT_LIGHT, 0);
    lv_obj_align(tagline, LV_ALIGN_CENTER, 0, 55);
    
    // Version info (larger font, below tagline)
    lv_obj_t *version = lv_label_create(splash);
    lv_label_set_text(version, "Version: " FLUIDTOUCH_VERSION);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(version, UITheme::TEXT_MEDIUM, 0);
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 85);
    
    // Force LVGL to render the splash screen
    lv_refr_now(disp);
    
    // Display splash for configured duration
    delay(SPLASH_DURATION_MS);
    
    // Delete splash screen
    lv_obj_del(splash);
}
