/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <time.h>
#include <stdint.h>
#include <stddef.h>

#include <esp_idf_version.h>
#ifndef CONFIG_IDF_TARGET_LINUX
#include <esp_sntp.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef CONFIG_IDF_TARGET_LINUX
/* For Linux targets, define a stub type for sntp_sync_time_cb_t */
typedef void (*sntp_sync_time_cb_t)(struct timeval *tv);
#endif

typedef struct esp_rmaker_time_config {
    /** If not specified, then 'CONFIG_ESP_RMAKER_SNTP_SERVER_NAME' is used as the SNTP server. */
    char *sntp_server_name;
    /** Optional callback to invoke, whenever time is synchronised. This will be called
     * periodically as per the SNTP polling interval (which is 60min by default).
     * If kept NULL, the default callback will be invoked, which will just print the
     * current local time.
     */
    sntp_sync_time_cb_t sync_time_cb;
} esp_rmaker_time_config_t;

/** Initialize time synchronization
 *
 * This API initializes SNTP for time synchronization.
 *
 * @param[in] config Configuration to be used for SNTP time synchronization. The default configuration is used if NULL is passed.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_sync_init(esp_rmaker_time_config_t *config);

/** Check if current time is updated
 *
 * This API checks if the current system time is updated against the reference time of 1-Jan-2019.
 *
 * @return true if time is updated
 * @return false if time is not updated
 */
bool esp_rmaker_time_check(void);

/** Wait for time synchronization
 *
 * This API waits for the system time to be updated against the reference time of 1-Jan-2019.
 * This is a blocking call.
 *
 * @param[in] ticks_to_wait Number of ticks to wait for time synchronization. Accepted values: 0 to portMAX_DELAY.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_wait_for_sync(uint32_t ticks_to_wait);

/** Parse ISO8601 UTC time string (eg. 2025-08-12T06:49:11Z) to epoch seconds
 *
 * This parser tolerates fractional seconds (eg. 2025-08-12T06:49:11.123Z) by stripping
 * the fractional part. Timezone offsets are also supported.
 *
 * @param[in] str Pointer to ISO8601 string (UTF-8)
 * @param[in] len Length of the string in bytes (if 0, function will use strlen)
 * @param[out] out_epoch Parsed epoch seconds (UTC)
 * @return ESP_OK on success, error otherwise
 */
esp_err_t esp_rmaker_time_convert_iso8601_to_epoch(const char *str, int len, time_t *out_epoch);

/** Set POSIX timezone
 *
 * Set the timezone (TZ environment variable) as per the POSIX format
 * specified in the [GNU libc documentation](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html).
 * Eg. For China: "CST-8"
 *     For US Pacific Time (including daylight saving information): "PST8PDT,M3.2.0,M11.1.0"
 *
 * @param[in] tz_posix NULL terminated TZ POSIX string
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_set_timezone_posix(const char *tz_posix);

/** Set timezone location string
 *
 * Set the timezone as a user friendly location string.
 * Check [here](https://rainmaker.espressif.com/docs/time-service.html) for a list of valid values.
 *
 * Eg. For China: "Asia/Shanghai"
 *     For US Pacific Time: "America/Los_Angeles"
 *
 * @note Setting timezone using this API internally also sets the POSIX timezone string.
 *
 * @param[in] tz NULL terminated Timezone location string
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_set_timezone(const char *tz);

/** Get the current POSIX timezone
 *
 * This fetches the current timezone in POSIX format, read from NVS.
 *
 * @return Pointer to a NULL terminated POSIX timezone string on success.
 *      Freeing this is the responsibility of the caller.
 * @return NULL on failure.
 */
char *esp_rmaker_time_get_timezone_posix(void);

/** Get the current timezone
 *
 * This fetches the current timezone in POSIX format, read from NVS.
 *
 * @return Pointer to a NULL terminated timezone string on success.
 *      Freeing this is the responsibility of the caller.
 * @return NULL on failure.
 */
char *esp_rmaker_time_get_timezone(void);

/** Get printable local time string
 *
 * Get a printable local time string, with information of timezone and Daylight Saving.
 * Eg. "Tue Sep  1 09:04:38 2020 -0400[EDT], DST: Yes"
 * "Tue Sep  1 21:04:04 2020 +0800[CST], DST: No"
 *
 *
 * @param[out] buf Pointer to a pre-allocated buffer into which the time string will
 *                  be populated.
 * @param[in] buf_len Length of the above buffer.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_get_local_time_str(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
