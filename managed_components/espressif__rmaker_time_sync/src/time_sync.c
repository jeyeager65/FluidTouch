/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <esp_log.h>
#include <nvs.h>
#include <esp_rmaker_time_sync.h>
#include <esp_rmaker_common_events.h>
#include <esp_idf_version.h>

#ifndef CONFIG_IDF_TARGET_LINUX
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
#include <esp_netif_sntp.h>
#else
#include <esp_sntp.h>
#endif
#endif

static const char *TAG = "esp_rmaker_time";

#define ESP_RMAKER_NVS_PART_NAME            "nvs"

#define ESP_RMAKER_NVS_TIME_NAMESPACE       "rmaker_time"
#define ESP_RMAKER_TZ_POSIX_NVS_NAME        "tz_posix"
#define ESP_RMAKER_TZ_NVS_NAME              "tz"

#define REF_TIME    1546300800 /* 01-Jan-2019 00:00:00 */
static bool init_done = false;
extern const char *esp_rmaker_tz_db_get_posix_str(const char *name);

#define ESP_RMAKER_DEF_TZ   CONFIG_ESP_RMAKER_DEF_TIMEZONE

int esp_setenv(const char *name, const char *value, int rewrite)
{
    return setenv(name, value, rewrite);
}

esp_err_t esp_rmaker_get_local_time_str(char *buf, size_t buf_len)
{
    struct tm timeinfo;
    char strftime_buf[64];
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c %z[%Z]", &timeinfo);
    size_t print_size = snprintf(buf, buf_len, "%s, DST: %s", strftime_buf, timeinfo.tm_isdst ? "Yes" : "No");
    if (print_size >= buf_len) {
        ESP_LOGE(TAG, "Buffer size %zu insufficient for localtime string. Required size: %zu", buf_len, print_size);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t esp_rmaker_print_current_time(void)
{
    char local_time[64];
    if (esp_rmaker_get_local_time_str(local_time, sizeof(local_time)) == ESP_OK) {
        if (esp_rmaker_time_check() == false) {
            ESP_LOGI(TAG, "Time not synchronised yet.");
        }
        ESP_LOGI(TAG, "The current time is: %s.", local_time);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static char *__esp_rmaker_time_get_nvs(const char *key)
{
    char *val = NULL;
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, ESP_RMAKER_NVS_TIME_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return NULL;
    }
    size_t len = 0;
    if ((err = nvs_get_blob(handle, key, NULL, &len)) == ESP_OK) {
        val = calloc(1, len + 1); /* +1 for NULL termination; small (TZ string), called rarely */
        if (val) {
            nvs_get_blob(handle, key, val, &len);
        }
    }
    nvs_close(handle);
    return val;

}

static esp_err_t __esp_rmaker_time_set_nvs(const char *key, const char *val)
{
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, ESP_RMAKER_NVS_TIME_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, key, val, strlen(val));
    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

char *esp_rmaker_time_get_timezone_posix(void)
{
    return __esp_rmaker_time_get_nvs(ESP_RMAKER_TZ_POSIX_NVS_NAME);
}

char *esp_rmaker_time_get_timezone(void)
{
    return __esp_rmaker_time_get_nvs(ESP_RMAKER_TZ_NVS_NAME);
}

esp_err_t esp_rmaker_time_set_timezone_posix(const char *tz_posix)
{
    // Basic validation for the POSIX timezone string
    if (!tz_posix || strlen(tz_posix) == 0) {
        ESP_LOGE(TAG, "Invalid POSIX timezone string: %s", tz_posix ? tz_posix : "NULL");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = __esp_rmaker_time_set_nvs(ESP_RMAKER_TZ_POSIX_NVS_NAME, tz_posix);
    if (err == ESP_OK) {
        esp_setenv("TZ", tz_posix, 1);
        tzset();
        esp_event_post(RMAKER_COMMON_EVENT, RMAKER_EVENT_TZ_POSIX_CHANGED,
                (void *)tz_posix, strlen(tz_posix) + 1, portMAX_DELAY);
        esp_rmaker_print_current_time();
    }
    return err;
}

esp_err_t esp_rmaker_time_set_timezone(const char *tz)
{
    const char *tz_posix = esp_rmaker_tz_db_get_posix_str(tz);
    if (!tz_posix) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_rmaker_time_set_timezone_posix(tz_posix);
    if (err == ESP_OK) {
        esp_event_post(RMAKER_COMMON_EVENT, RMAKER_EVENT_TZ_CHANGED, (void *)tz,
                strlen(tz) + 1, portMAX_DELAY);
        err = __esp_rmaker_time_set_nvs(ESP_RMAKER_TZ_NVS_NAME, tz);
    }
    return err;
}

esp_err_t esp_rmaker_timezone_enable(void)
{
#ifdef CONFIG_IDF_TARGET_LINUX
    ESP_LOGI(TAG, "Timezone enable skipped for Linux target");
    return ESP_OK;
#else
    char *tz_posix = esp_rmaker_time_get_timezone_posix();
    if (tz_posix) {
        esp_setenv("TZ", tz_posix, 1);
        tzset();
        free(tz_posix);
    } else {
        if (strlen(ESP_RMAKER_DEF_TZ) > 0) {
            const char *tz_def = esp_rmaker_tz_db_get_posix_str(ESP_RMAKER_DEF_TZ);
            if (tz_def) {
                esp_setenv("TZ", tz_def, 1);
                tzset();
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "Invalid Timezone %s specified.", ESP_RMAKER_DEF_TZ);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    return ESP_OK;
#endif
}

#ifndef CONFIG_IDF_TARGET_LINUX
static void esp_rmaker_time_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP Synchronised.");
    esp_rmaker_print_current_time();
}
#endif

esp_err_t esp_rmaker_time_sync_init(esp_rmaker_time_config_t *config)
{
#ifdef CONFIG_IDF_TARGET_LINUX
    ESP_LOGI(TAG, "SNTP initialization skipped for Linux target");
    init_done = true;
    return ESP_OK;
#else
    if (esp_sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already initialized.");
        init_done = true;
        return ESP_OK;
    }
    const char *sntp_server_name;
    if (!config || !config->sntp_server_name) {
        sntp_server_name = CONFIG_ESP_RMAKER_SNTP_SERVER_NAME;
    } else {
        sntp_server_name = config->sntp_server_name;
    }
    ESP_LOGI(TAG, "Initializing SNTP. Using the SNTP server: %s", sntp_server_name);

    void (*sync_notification_cb)(struct timeval *tv);
    if (config && config->sync_time_cb) {
        sync_notification_cb = config->sync_time_cb;
    } else {
        sync_notification_cb = esp_rmaker_time_sync_cb;
    }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG(sntp_server_name);
    sntp_config.start = true;                       // start SNTP service at init (polls until network is up)
#ifdef CONFIG_LWIP_DHCP_GET_NTP_SRV
    sntp_config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
    sntp_config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
    sntp_config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
#endif
    sntp_config.sync_cb = sync_notification_cb; // only if we need the notification function
    esp_netif_sntp_init(&sntp_config);
#else
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, sntp_server_name);
    esp_sntp_init();
    sntp_set_time_sync_notification_cb(sync_notification_cb);
#endif

    esp_rmaker_timezone_enable();
    init_done = true;
    return ESP_OK;
#endif
}

bool esp_rmaker_time_check(void)
{
    time_t now;
    time(&now);
    if (now > REF_TIME) {
        return true;
    }
    return false;
}


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>

/* ---- UTC converter with no setenv/timegm dependency ---- */
static inline int is_leap(int y) {
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

/* Interprets tm as UTC and returns epoch seconds */
static inline time_t rmaker_timegm(struct tm *tm)
{
    /* Normalize month to [0,11] */
    int year  = tm->tm_year + 1900;
    int month = tm->tm_mon;
    if (month < 0) { year += (month - 11) / 12; month = 12 + (month % 12); }
    else if (month > 11) { year += month / 12; month %= 12; }

    static const int mdays_cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};

    int64_t days = 0;
    if (year >= 1970) for (int y = 1970; y < year; ++y) days += 365 + is_leap(y);
    else              for (int y = year;  y < 1970; ++y) days -= 365 + is_leap(y);

    days += mdays_cum[month];
    if (month > 1 && is_leap(year)) days += 1;
    days += (tm->tm_mday - 1);

    int64_t seconds = days * 86400LL
                    + (int64_t)tm->tm_hour * 3600
                    + (int64_t)tm->tm_min  * 60
                    + (int64_t)tm->tm_sec;

    return (time_t)seconds;
}

esp_err_t esp_rmaker_time_convert_iso8601_to_epoch(const char *str, int len, time_t *out_epoch)
{
    if (!str || !out_epoch) {
        return ESP_FAIL;
    }

    char buf[64];
    int copy_len = 0;
    if (len > 0) {
        copy_len = (len < (int)sizeof(buf) - 1) ? len : (int)sizeof(buf) - 1;
    } else {
        size_t str_len = strlen(str);
        copy_len = (str_len < sizeof(buf) - 1) ? (int)str_len : (int)sizeof(buf) - 1;
    }
    memcpy(buf, str, copy_len);
    buf[copy_len] = '\0';

    struct tm tm_utc = {0};
    int year, month, day, hour, minute, second;
    int tz_hour = 0, tz_minute = 0;
    char tz_sign = '+';

    /* Number of fields expected for each format */
    #define ISO8601_WITH_TIMEZONE_FIELDS    9  /* year, month, day, hour, minute, second, tz_sign, tz_hour, tz_minute */
    #define ISO8601_UTC_FIELDS              6  /* year, month, day, hour, minute, second */

    /* Try parsing with timezone offset: "YYYY-MM-DDTHH:MM:SS[+-]HH:MM" */
    if (sscanf(buf, "%d-%d-%dT%d:%d:%d%c%d:%d",
               &year, &month, &day, &hour, &minute, &second,
               &tz_sign, &tz_hour, &tz_minute) == ISO8601_WITH_TIMEZONE_FIELDS) {
        /* Timezone offset format parsed successfully */
    } else if (sscanf(buf, "%d-%d-%dT%d:%d:%dZ",
                      &year, &month, &day, &hour, &minute, &second) == ISO8601_UTC_FIELDS) {
        /* UTC format - validate that 'Z' is present at position 19 */
        size_t buf_len = strlen(buf);
        if (buf_len < 20 || buf[19] != 'Z') {
            ESP_LOGE(TAG, "Invalid ISO 8601 format: '%s' (missing Z suffix)", buf);
            return ESP_FAIL;
        }
        tz_hour = 0;
        tz_minute = 0;
        tz_sign = '+';
    } else {
        ESP_LOGE(TAG, "Invalid ISO 8601 format: '%s'", buf);
        return ESP_FAIL;
    }

    /* Fill tm structure with parsed values */
    tm_utc.tm_year = year - 1900;
    tm_utc.tm_mon = month - 1;
    tm_utc.tm_mday = day;
    tm_utc.tm_hour = hour;
    tm_utc.tm_min = minute;
    tm_utc.tm_sec = second;
    tm_utc.tm_isdst = 0; /* No DST in UTC */

    /* Calculate timezone offset in seconds */
    int total_offset_seconds = (tz_hour * 3600) + (tz_minute * 60);
    if (tz_sign == '-') {
        total_offset_seconds = -total_offset_seconds;
    }

    /* Convert to epoch time using timezone-independent rmaker_timegm */
    time_t utc_epoch = rmaker_timegm(&tm_utc);
    if (utc_epoch == (time_t)-1) {
        ESP_LOGE(TAG, "rmaker_timegm failed");
        return ESP_FAIL;
    }

    /* Adjust for timezone offset to get true epoch time */
    *out_epoch = utc_epoch - total_offset_seconds;

    return ESP_OK;
}

#define DEFAULT_TICKS   (2000 / portTICK_PERIOD_MS) /* 2 seconds in ticks */

esp_err_t esp_rmaker_time_wait_for_sync(uint32_t ticks_to_wait)
{
    if (!init_done) {
        ESP_LOGW(TAG, "Time sync not initialized using 'esp_rmaker_time_sync_init'");
    }
    ESP_LOGW(TAG, "Waiting for time to be synchronized. This may take time.");
    uint32_t ticks_remaining = ticks_to_wait;
    uint32_t ticks = DEFAULT_TICKS;
    while (ticks_remaining > 0) {
        if (esp_rmaker_time_check() == true) {
            break;
        }
        ESP_LOGD(TAG, "Time not synchronized yet. Retrying...");
        ticks = ticks_remaining < DEFAULT_TICKS ? ticks_remaining : DEFAULT_TICKS;
        ticks_remaining -= ticks;
        vTaskDelay(ticks);
    }

    /* Check if ticks_to_wait expired and time is not synchronized yet. */
    if (esp_rmaker_time_check() == false) {
        ESP_LOGE(TAG, "Time not synchronized within the provided ticks: %" PRIu32, ticks_to_wait);
        return ESP_FAIL;
    }

    /* Get current time */
    esp_rmaker_print_current_time();
    return ESP_OK;
}
