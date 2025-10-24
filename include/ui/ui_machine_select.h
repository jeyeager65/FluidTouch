#ifndef UI_MACHINE_SELECT_H
#define UI_MACHINE_SELECT_H

#include <lvgl.h>

class UIMachineSelect {
public:
    static void show(lv_display_t *disp);
    static void hide();
    
private:
    static lv_obj_t *screen;
    static lv_display_t *display;
    static void onMachineSelected(lv_event_t *e);
};

#endif // UI_MACHINE_SELECT_H
