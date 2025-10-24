#ifndef UI_TAB_STATUS_H
#define UI_TAB_STATUS_H

#include <lvgl.h>

class UITabStatus {
public:
    static void create(lv_obj_t *tab);
    static void updateMessage(const char *message);
    
private:
    static lv_obj_t *lbl_message;
};

#endif // UI_TAB_STATUS_H
