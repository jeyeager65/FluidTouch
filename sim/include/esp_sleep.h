// Desktop stub for ESP-IDF deep sleep. esp_deep_sleep_start() exits the
// simulator (on hardware only the reset button wakes the device).
#pragma once

typedef enum { ESP_SLEEP_WAKEUP_ALL = 0, ESP_SLEEP_WAKEUP_EXT0, ESP_SLEEP_WAKEUP_EXT1,
               ESP_SLEEP_WAKEUP_TIMER, ESP_SLEEP_WAKEUP_TOUCHPAD } esp_sleep_source_t;
typedef enum { ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_DOMAIN_RTC_FAST_MEM,
               ESP_PD_DOMAIN_XTAL } esp_sleep_pd_domain_t;
typedef enum { ESP_PD_OPTION_OFF, ESP_PD_OPTION_ON, ESP_PD_OPTION_AUTO } esp_sleep_pd_option_t;
typedef int esp_err_t;

inline esp_err_t esp_sleep_disable_wakeup_source(esp_sleep_source_t) { return 0; }
inline esp_err_t esp_sleep_pd_config(esp_sleep_pd_domain_t, esp_sleep_pd_option_t) { return 0; }
inline esp_err_t esp_sleep_enable_ext0_wakeup(int, int) { return 0; }
inline esp_err_t esp_sleep_enable_timer_wakeup(unsigned long long) { return 0; }

namespace sim { [[noreturn]] void deepSleep(); }
inline void esp_deep_sleep_start() { sim::deepSleep(); }
