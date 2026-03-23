#ifndef UI_TAB_CONTROL_JOG_H
#define UI_TAB_CONTROL_JOG_H

#include <lvgl.h>

class UITabControlJog {
public:
    static void create(lv_obj_t *tab);

private:
    static lv_obj_t *xy_step_display_label;
    static lv_obj_t *z_step_display_label;
    static lv_obj_t *xy_step_buttons[5];
    static lv_obj_t *z_step_buttons[5];
    static lv_obj_t *xy_feedrate_label;
    static lv_obj_t *z_feedrate_label;
    static float xy_current_step;
    static float z_current_step;
    static int xy_current_step_index;
    static int z_current_step_index;
    static int xy_current_feed;
    static int z_current_feed;

    // Step value arrays (parsed from settings) - max 5 values per axis
    static float xy_step_values[5];
    static float z_step_values[5];
    static float a_step_values[5];
    static int xy_step_count;
    static int z_step_count;
    static int a_step_count;

    // Z/A Toggle (only created if A-axis enabled)
    static lv_obj_t *btn_z_mode;         // Z button in segmented control
    static lv_obj_t *btn_a_mode;         // A button in segmented control
    static lv_obj_t *za_header;          // Header label that says "Z JOG" or "A JOG"
    static bool is_z_mode;               // true = Z mode, false = A mode

    // A-axis control references (parallels Z-axis structure)
    static lv_obj_t *a_step_label;        // "A Step" text
    static lv_obj_t *a_step_display_label;
    static lv_obj_t *a_step_buttons[5];
    static lv_obj_t *a_feedrate_label;
    static lv_obj_t *btn_a_up;            // A+ button
    static lv_obj_t *btn_a_down;          // A- button
    static lv_obj_t *a_step_display_bg;   // Background for step display
    static lv_obj_t *a_feed_label;        // "A Feed:" text
    static lv_obj_t *a_feed_unit;         // "mm/min" text
    static lv_obj_t *a_feed_minus1000_btn;
    static lv_obj_t *a_feed_minus100_btn;
    static lv_obj_t *a_feed_plus100_btn;
    static lv_obj_t *a_feed_plus1000_btn;

    // A-axis state (parallels Z-axis variables)
    static float a_current_step;
    static int a_current_step_index;
    static int a_current_feed;

    // Z step label (needs to be stored for visibility toggle)
    static lv_obj_t *z_step_label;
    static lv_obj_t *z_feed_label;
    static lv_obj_t *z_feed_unit;
    static lv_obj_t *btn_z_up;
    static lv_obj_t *btn_z_down;
    static lv_obj_t *z_step_display_bg;
    static lv_obj_t *btn_z_minus1000;
    static lv_obj_t *btn_z_minus100;
    static lv_obj_t *btn_z_plus100;
    static lv_obj_t *btn_z_plus1000;

    // Octagon stop button
    static void draw_octagon_event_cb(lv_event_t *e);
    static void xy_step_button_event_cb(lv_event_t *e);
    static void z_step_button_event_cb(lv_event_t *e);
    static void xy_feedrate_adj_event_cb(lv_event_t *e);
    static void z_feedrate_adj_event_cb(lv_event_t *e);
    static void update_xy_step_display();
    static void update_z_step_display();
    static void update_xy_step_button_styles();
    static void update_z_step_button_styles();

    // A-axis event handlers (parallels Z-axis handlers)
    static void a_step_button_event_cb(lv_event_t *e);
    static void a_jog_button_event_cb(lv_event_t *e);
    static void a_feedrate_adj_event_cb(lv_event_t *e);
    static void update_a_step_button_styles();
    static void update_a_step_display();

    // Toggle event handler
    static void za_toggle_event_cb(lv_event_t *e);

    // Jog button event handlers
    static void xy_jog_button_event_cb(lv_event_t *e);
    static void z_jog_button_event_cb(lv_event_t *e);
    static void cancel_jog_event_cb(lv_event_t *e);

    // Step value parsing
    static void parseStepValues();
    
    // Helper function to find closest step index
    static int findClosestStepIndex(const float* step_values, int step_count, float target_value);
    
    // Helper function to format step values with minimal decimal places
    static void formatStepValue(float value, char* buffer, size_t buffer_size);
};

#endif // UI_TAB_CONTROL_JOG_H
