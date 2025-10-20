#include "ui/tabs/ui_tab_terminal.h"

void UITabTerminal::create(lv_obj_t *tab) {
    // Set dark background
    lv_obj_set_style_bg_color(tab, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
    
    // Terminal output container - full screen
    lv_obj_t *terminal_cont = lv_obj_create(tab);
    lv_obj_set_size(terminal_cont, 765, 342);  // Full width with margins
    lv_obj_set_pos(terminal_cont, 0, 0);
    lv_obj_set_style_bg_color(terminal_cont, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_color(terminal_cont, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_border_width(terminal_cont, 2, LV_PART_MAIN);

    lv_obj_t *terminal_text = lv_label_create(terminal_cont);
    lv_label_set_text(terminal_text, 
        "[MSG:INFO: FluidNC v3.9.4 https://github.com/bdring/FluidNC.git]\n"
        "[MSG:INFO: Compiled with ESP32 SDK:v4.4.7-dirty]\n"
        "[MSG:INFO: Local filesystem type is spiffs]\n"
        "[MSG:INFO: Configuration file:config.yaml]\n"
        "[MSG:INFO: Machine WallPlotter]\n"
        "[MSG:INFO: Board Jackpot TMC2209]\n"
        "[MSG:INFO: UART1 Tx:gpio.0 Rx:gpio.4 RTS:NO_PIN Baud:115200]\n"
        "[MSG:INFO: I2SO BCK:gpio.22 WS:gpio.17 DATA:gpio.21Min Pulse:2us]\n"
        "[MSG:INFO: SPI SCK:gpio.18 MOSI:gpio.23 MISO:gpio.19]\n"
        "[MSG:INFO: SD Card cs_pin:gpio.5 detect:NO_PIN freq:20000000]\n"
        "[MSG:INFO: Stepping:I2S_STATIC Pulse:4us Dsbl Delay:0us Dir Delay:1us Idle Delay:255ms]\n"
        "[MSG:INFO: User Digital Output: 0 on Pin:gpio.26]\n"
        "[MSG:INFO: Axis count 3]\n"
        "[MSG:INFO: Axis X (0.000,640.000)]\n"
        "[MSG:INFO:   Motor0]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:0 CS:NO_PIN Step:I2SO.2 Dir:I2SO.1 Disable:I2SO.0 R:0.110]\n"
        "[MSG:INFO:  X Neg Limit gpio.25]\n"
        "[MSG:INFO:   Motor1]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:3 CS:I2SO.14 Step:I2SO.13 Dir:I2SO.12 Disable:I2SO.15 R:0.110]\n"
        "[MSG:INFO: Axis Y (0.000,625.000)]\n"
        "[MSG:INFO:   Motor0]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:1 CS:NO_PIN Step:I2SO.5 Dir:I2SO.4 Disable:I2SO.7 R:0.110]\n"
        "[MSG:INFO:  Y Neg Limit gpio.33]\n"
        "[MSG:INFO: Axis Z (-16.000,0.000)]\n"
        "[MSG:INFO:   Motor0]\n"
        "[MSG:INFO:     tmc_2209 UART1 Addr:2 CS:NO_PIN Step:I2SO.10 Dir:I2SO.9 Disable:I2SO.8 R:0.110]\n"
        "[MSG:INFO:  Z Pos Limit gpio.32]\n"
        "[MSG:INFO: X Axis driver test passed]\n"
        "[MSG:INFO: X2 Axis driver test passed]\n"
        "[MSG:INFO: Y Axis driver test passed]\n"
        "[MSG:INFO: Z Axis driver test passed]\n"
        "[MSG:INFO: Kinematic system: Cartesian]\n"
        "[MSG:INFO: Connecting to STA SSID:YEAGER]\n"
        "[MSG:INFO: Connecting.]\n"
        "[MSG:INFO: Connecting..]\n"
        "[MSG:INFO: Connected - IP is 192.168.0.220]\n"
        "[MSG:INFO: WiFi on]\n"
        "[MSG:INFO: Start mDNS with hostname:http://penplotter.local/]\n"
        "[MSG:INFO: HTTP started on port 80]\n"
        "[MSG:INFO: Telnet started on port 23]\n"
        "[MSG:INFO: BESC Spindle Out:gpio.27 Min:640us Max:1150us Freq:50Hz Full Period count:1048575 with m6_macro]\n"
        "[MSG:INFO: Probe gpio.36:low]\n"
        "ok\n"
        "<Idle|MPos:0.000,0.000,0.000|FS:0,0>");
    lv_obj_set_style_text_font(terminal_text, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(terminal_text, lv_color_hex(0x00FF00), 0);
    lv_obj_set_pos(terminal_text, 0, 0);
}
