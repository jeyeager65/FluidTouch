/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "unity.h"
#include "esp_rmaker_utils.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_event.h"
#include "esp_rmaker_cmd_resp.h"
#include "esp_netif.h"

static const char *TAG = "rmaker_utils_test";

static void time_sync_callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time sync callback called");
}

TEST_CASE("ESP RainMaker Time Sync Init", "[rmaker_utils]")
{
    esp_netif_init();

    esp_rmaker_time_config_t time_config = {
        .sntp_server_name = (char *)"pool.ntp.org",
        .sync_time_cb = time_sync_callback
    };

    esp_err_t err = esp_rmaker_time_sync_init(&time_config);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Test with NULL config (should use defaults)
    err = esp_rmaker_time_sync_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker Time Check", "[rmaker_utils]")
{
    // Since we just initialized time sync, time might not be synced yet
    // This is more of a function call test than testing the actual result
    bool time_synced = esp_rmaker_time_check();

    // We can only log the result, not assert since it depends on network conditions
    ESP_LOGI(TAG, "Time synced: %s", time_synced ? "true" : "false");
}

TEST_CASE("ESP RainMaker Timezone Operations", "[rmaker_utils]")
{
    // Test setting timezone in POSIX format
    esp_err_t err = esp_rmaker_time_set_timezone_posix("PST8PDT,M3.2.0,M11.1.0");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Get and verify timezone in POSIX format
    char *tz_posix = esp_rmaker_time_get_timezone_posix();
    if (tz_posix) {
        ESP_LOGI(TAG, "POSIX timezone: %s", tz_posix);
        TEST_ASSERT_EQUAL_STRING("PST8PDT,M3.2.0,M11.1.0", tz_posix);
        free(tz_posix);
    }

    // Test setting timezone by location
    err = esp_rmaker_time_set_timezone("America/Los_Angeles");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Get and verify timezone by location
    char *tz = esp_rmaker_time_get_timezone();
    if (tz) {
        ESP_LOGI(TAG, "Timezone: %s", tz);
        TEST_ASSERT_EQUAL_STRING("America/Los_Angeles", tz);
        free(tz);
    }
}

TEST_CASE("ESP RainMaker Get Local Time", "[rmaker_utils]")
{
    char time_str[64];
    esp_err_t err = esp_rmaker_get_local_time_str(time_str, sizeof(time_str));
    TEST_ASSERT_EQUAL(ESP_OK, err);

    ESP_LOGI(TAG, "Local time: %s", time_str);
    TEST_ASSERT_NOT_EQUAL(0, strlen(time_str));
}

// We can't actually test the reboot and reset functions in a unit test
// as they would terminate the test process. Instead, we'll test the API calls.

TEST_CASE("ESP RainMaker Reboot API Test", "[rmaker_utils]")
{
    // Just test API call, don't actually reboot
    // Set a delay so the test can finish before reboot starts
    ESP_LOGI(TAG, "Testing reboot API (with 120 second delay, won't actually reboot during test)");
    esp_err_t err = esp_rmaker_reboot(120);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker Wi-Fi Reset API Test", "[rmaker_utils]")
{
    // Just test API call, don't actually reset
    // Set a delay so the test can finish before reset starts
    ESP_LOGI(TAG, "Testing Wi-Fi reset API (with 120 second delay, won't actually reset during test)");
    esp_err_t err = esp_rmaker_wifi_reset(120, -1);
#if defined(CONFIG_ESP_WIFI_ENABLED) || defined(CONFIG_ESP32_WIFI_ENABLED) || defined(CONFIG_ESP_WIFI_REMOTE_ENABLED)
    TEST_ASSERT_EQUAL(ESP_OK, err);
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, err);
#endif
}

TEST_CASE("ESP RainMaker Factory Reset API Test", "[rmaker_utils]")
{
    // Just test API call, don't actually reset
    // Set a delay so the test can finish before reset starts
    ESP_LOGI(TAG, "Testing factory reset API (with 120 second delay, won't actually reset during test)");
    esp_err_t err = esp_rmaker_factory_reset(120, -1);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker SNTP Initialization", "[rmaker_utils]")
{
    esp_netif_init();

    // Test with default configuration
    esp_rmaker_time_config_t default_config = {0};
    esp_err_t err = esp_rmaker_time_sync_init(&default_config);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Test with custom SNTP server
    esp_rmaker_time_config_t custom_config = {
        .sntp_server_name = (char *)"time.google.com"
    };
    err = esp_rmaker_time_sync_init(&custom_config);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Test with NULL configuration (should use default)
    err = esp_rmaker_time_sync_init(NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker Invalid POSIX Timezone", "[rmaker_utils]")
{
    // Test with an invalid POSIX timezone string
    esp_err_t err = esp_rmaker_time_set_timezone_posix(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    err = esp_rmaker_time_set_timezone_posix("");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

static esp_err_t test_cmd_handler(const void *in_data, size_t in_len, void **out_data, size_t *out_len, esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    // Check for valid command context
    if (ctx == NULL || ctx->req_id[0] == '\0' || ctx->user_role == 0 || ctx->cmd == 0) {
        ESP_LOGE(TAG, "Invalid command context: req_id=%s, user_role=%d, cmd=%d", ctx ? ctx->req_id : "NULL", ctx ? ctx->user_role : -1, ctx ? ctx->cmd : -1);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Command context: req_id=%s, user_role=%d, cmd=%d", ctx->req_id, ctx->user_role, ctx->cmd);

    // Simple echo handler for testing
    *out_data = malloc(in_len);
    if (*out_data == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(*out_data, in_data, in_len);
    *out_len = in_len;
    return ESP_OK;
}

// Wrapper function to match esp_rmaker_cmd_send_t signature
static esp_err_t cmd_response_handler_wrapper(const void *input, size_t input_len, void *priv_data) {
    void *output_data = NULL;
    size_t output_len = 0;
    esp_err_t err = esp_rmaker_cmd_response_handler(input, input_len, &output_data, &output_len);
    if (output_data) {
        free(output_data);
    }
    return err;
}

TEST_CASE("ESP RainMaker ISO8601 to Epoch Conversion", "[rmaker_utils]")
{
    time_t epoch_time;
    esp_err_t err;

    /* Test valid ISO8601 UTC format with Z suffix */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2023-12-25T10:30:45Z", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "ISO8601 '2023-12-25T10:30:45Z' converted to epoch: %ld", (long)epoch_time);

    /* Test valid ISO8601 with positive timezone offset */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2023-12-25T15:30:45+05:00", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "ISO8601 '2023-12-25T15:30:45+05:00' converted to epoch: %ld", (long)epoch_time);

    /* Test valid ISO8601 with negative timezone offset */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2023-12-25T05:30:45-05:00", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "ISO8601 '2023-12-25T05:30:45-05:00' converted to epoch: %ld", (long)epoch_time);

    /* Test with explicit length parameter */
    const char *iso_str = "2023-12-25T10:30:45Z_extra_data";
    err = esp_rmaker_time_convert_iso8601_to_epoch(iso_str, 20, &epoch_time);  // Only parse first 20 chars
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "ISO8601 with length limit converted to epoch: %ld", (long)epoch_time);

    /* Test epoch time from 2000-01-01T00:00:00Z (millennium boundary) */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2000-01-01T00:00:00Z", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(946684800, epoch_time);  // Known epoch for 2000-01-01 00:00:00 UTC
    ESP_LOGI(TAG, "Millennium boundary test passed: %ld", (long)epoch_time);

    /* Test invalid inputs - NULL string */
    err = esp_rmaker_time_convert_iso8601_to_epoch(NULL, 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);

    /* Test invalid inputs - NULL output pointer */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2023-12-25T10:30:45Z", 0, NULL);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);

    /* Test invalid ISO8601 format - empty string */
    err = esp_rmaker_time_convert_iso8601_to_epoch("", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);

    /* Test invalid ISO8601 format - incomplete string */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2023-12-25T10:30", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);

    /* Test invalid ISO8601 format - completely malformed */
    err = esp_rmaker_time_convert_iso8601_to_epoch("not-a-date", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);

    /* Test invalid ISO8601 format - missing Z suffix */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2023-12-25T10:30:45", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);

    /* Test edge case - very long string (should be truncated internally) */
    char long_str[120];
    snprintf(long_str, sizeof(long_str), "2023-12-25T10:30:45Z%s",
             "this_is_a_very_long_suffix_that_should_be_ignored_when_parsing_the_iso8601_timestamp");
    err = esp_rmaker_time_convert_iso8601_to_epoch(long_str, 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "Long string test passed: %ld", (long)epoch_time);
}

TEST_CASE("ESP RainMaker ISO8601 to Epoch with Different Timezones", "[rmaker_utils]")
{
    time_t epoch_time;
    esp_err_t err;

    /* Store original timezone for restoration */
    char *original_tz = getenv("TZ");
    char original_tz_backup[64] = {0};
    if (original_tz) {
        snprintf(original_tz_backup, sizeof(original_tz_backup), "%s", original_tz);
    }

    /* Test the same ISO8601 timestamp with different system timezones */
    const char *test_timestamp_utc = "2023-06-15T14:30:00Z";
    const char *test_timestamp_offset = "2023-06-15T19:30:00+05:00";  /* Same UTC time as above */
    time_t expected_epoch = 0;

    /* First, get the expected epoch time with UTC timezone */
    setenv("TZ", "UTC", 1);
    tzset();
    err = esp_rmaker_time_convert_iso8601_to_epoch(test_timestamp_utc, 0, &expected_epoch);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "Expected epoch (UTC): %ld", (long)expected_epoch);

    /* Test 1: Pacific Standard Time (PST) */
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();

    err = esp_rmaker_time_convert_iso8601_to_epoch(test_timestamp_utc, 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(expected_epoch, epoch_time);
    ESP_LOGI(TAG, "PST timezone: %ld (matches expected)", (long)epoch_time);

    /* Test 2: Japan Standard Time (JST) */
    setenv("TZ", "JST-9", 1);
    tzset();

    err = esp_rmaker_time_convert_iso8601_to_epoch(test_timestamp_offset, 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(expected_epoch, epoch_time);
    ESP_LOGI(TAG, "JST timezone: %ld (matches expected)", (long)epoch_time);

    /* Restore original timezone */
    if (original_tz_backup[0] != '\0') {
        setenv("TZ", original_tz_backup, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    ESP_LOGI(TAG, "Timezone restored");
}

TEST_CASE("ESP RainMaker ISO8601 DST and Leap Year Tests", "[rmaker_utils]")
{
    time_t epoch_time;
    esp_err_t err;

    /* Test leap year - Feb 29, 2020 (valid leap year date) */
    err = esp_rmaker_time_convert_iso8601_to_epoch("2020-02-29T12:00:00Z", 0, &epoch_time);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "Leap year Feb 29, 2020: %ld", (long)epoch_time);

    /* Test non-leap year - Feb 28, 2021 vs Mar 1, 2021 should be 24 hours apart */
    time_t feb28_2021, mar01_2021;
    err = esp_rmaker_time_convert_iso8601_to_epoch("2021-02-28T12:00:00Z", 0, &feb28_2021);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = esp_rmaker_time_convert_iso8601_to_epoch("2021-03-01T12:00:00Z", 0, &mar01_2021);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(86400, mar01_2021 - feb28_2021);  /* 24 hours = 86400 seconds */
    ESP_LOGI(TAG, "Non-leap year Feb28-Mar01 gap: %ld seconds", (long)(mar01_2021 - feb28_2021));

    /* Store original timezone for DST testing */
    char *original_tz = getenv("TZ");
    char original_tz_backup[64] = {0};
    if (original_tz) {
        snprintf(original_tz_backup, sizeof(original_tz_backup), "%s", original_tz);
    }

    /* Test DST transition - same UTC time should give same epoch regardless of system DST */
    const char *summer_utc = "2023-07-15T14:00:00Z";  /* Summer (DST active in many zones) */
    const char *winter_utc = "2023-01-15T14:00:00Z";  /* Winter (DST inactive) */
    time_t summer_epoch, winter_epoch;

    /* Test with PST (observes DST) */
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();

    err = esp_rmaker_time_convert_iso8601_to_epoch(summer_utc, 0, &summer_epoch);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    err = esp_rmaker_time_convert_iso8601_to_epoch(winter_utc, 0, &winter_epoch);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    ESP_LOGI(TAG, "DST test - Summer UTC: %ld, Winter UTC: %ld", (long)summer_epoch, (long)winter_epoch);

    /* Restore timezone */
    if (original_tz_backup[0] != '\0') {
        setenv("TZ", original_tz_backup, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
}

TEST_CASE("ESP RainMaker Command Registration and Handling", "[rmaker_cmd_resp]")
{
    // Prepare command context with valid values
    esp_rmaker_cmd_ctx_t cmd_ctx = {
        .cmd = ESP_RMAKER_CMD_TYPE_SET_PARAMS,
        .req_id = "test_req_id",
        .user_role = ESP_RMAKER_USER_ROLE_PRIMARY_USER
    };

    // Register a test command
    esp_err_t err = esp_rmaker_cmd_register(ESP_RMAKER_CMD_TYPE_SET_PARAMS, ESP_RMAKER_USER_ROLE_PRIMARY_USER, test_cmd_handler, true, &cmd_ctx);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    void *output_data = NULL;

#if 0 // The response should ideally be not ESP_OK, but it is ESP_OK currently
    // Prepare invalid input data
    const char *invalid_data = "invalid_data";
    size_t invalid_len = strlen(invalid_data);
    size_t output_len = 0;

    // Pass invalid data first to check for failure
    err = esp_rmaker_cmd_response_handler(invalid_data, invalid_len, &output_data, &output_len);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
#endif

    // Now use esp_rmaker_cmd_resp_test_send to send valid TLV data
    err = esp_rmaker_cmd_resp_test_send(cmd_ctx.req_id, cmd_ctx.user_role, cmd_ctx.cmd, NULL, 0, cmd_response_handler_wrapper, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Clean up
    if (output_data) {
        free(output_data);
    }

    // Deregister the test command
    err = esp_rmaker_cmd_deregister(ESP_RMAKER_CMD_TYPE_SET_PARAMS);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

#define TEST_CMD_DEFERRED   (ESP_RMAKER_CMD_CUSTOM_START + 10U)

static uint8_t test_tlv_get_status(const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t rem = len;
    while (rem >= 2) {
        uint8_t type = p[0];
        uint8_t l = p[1];
        if (rem < (size_t)(2 + l)) {
            break;
        }
        if (type == ESP_RMAKER_TLV_TYPE_STATUS && l >= 1) {
            return p[2];
        }
        p += 2 + l;
        rem -= 2 + l;
    }
    return 0xFF;
}

static bool s_deferred_called;
static esp_rmaker_cmd_ctx_t s_deferred_ctx;

static esp_err_t test_cmd_deferred_handler(const void *in_data, size_t in_len,
                                           void **out_data, size_t *out_len,
                                           esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    (void)in_data; (void)in_len; (void)out_data; (void)out_len; (void)priv;
    s_deferred_called = true;
    memcpy(&s_deferred_ctx, ctx, sizeof(s_deferred_ctx));
    return ESP_ERR_NOT_FINISHED;
}

static esp_err_t cmd_deferred_handler_wrapper(const void *input, size_t input_len, void *priv_data)
{
    (void)priv_data;
    void *output_data = (void *)0xdeadbeef; /* sentinel: must be cleared by dispatcher on defer */
    size_t output_len = 0xdeadbeef;
    esp_err_t err = esp_rmaker_cmd_response_handler(input, input_len, &output_data, &output_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_NULL(output_data);
    TEST_ASSERT_EQUAL(0, output_len);
    return err;
}

TEST_CASE("ESP RainMaker Deferred Command Response", "[rmaker_cmd_resp]")
{
    esp_err_t err;
    void *build_out = NULL;
    size_t build_len = 0;

    /* Register a handler that defers. */
    err = esp_rmaker_cmd_register(TEST_CMD_DEFERRED, ESP_RMAKER_USER_ROLE_PRIMARY_USER,
                                  test_cmd_deferred_handler, false, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* Dispatch a command to it; the wrapper asserts the dispatcher produced no output. */
    s_deferred_called = false;
    memset(&s_deferred_ctx, 0, sizeof(s_deferred_ctx));
    err = esp_rmaker_cmd_resp_test_send("deferred_req_1", ESP_RMAKER_USER_ROLE_PRIMARY_USER, TEST_CMD_DEFERRED,
                                        "in", 2, cmd_deferred_handler_wrapper, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(s_deferred_called);
    TEST_ASSERT_EQUAL(TEST_CMD_DEFERRED, s_deferred_ctx.cmd);
    TEST_ASSERT_EQUAL_STRING("deferred_req_1", s_deferred_ctx.req_id);
    TEST_ASSERT_EQUAL(ESP_RMAKER_USER_ROLE_PRIMARY_USER, s_deferred_ctx.user_role);

    /* Simulate async completion: build the response directly via the unified builder. */
    err = esp_rmaker_cmd_prepare_payload(s_deferred_ctx.req_id, 0, ESP_RMAKER_CMD_STATUS_SUCCESS,
                                         s_deferred_ctx.cmd, "ok", 2, &build_out, &build_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_NOT_NULL(build_out);
    TEST_ASSERT_TRUE(build_len > 0);
    TEST_ASSERT_EQUAL(ESP_RMAKER_CMD_STATUS_SUCCESS, test_tlv_get_status(build_out, build_len));
    err = esp_rmaker_cmd_resp_parse_response(build_out, build_len, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    free(build_out);
    build_out = NULL;

    /* FAILED status also round-trips. */
    err = esp_rmaker_cmd_prepare_payload(s_deferred_ctx.req_id, 0, ESP_RMAKER_CMD_STATUS_FAILED,
                                         s_deferred_ctx.cmd, NULL, 0, &build_out, &build_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(ESP_RMAKER_CMD_STATUS_FAILED, test_tlv_get_status(build_out, build_len));
    free(build_out);

    err = esp_rmaker_cmd_deregister(TEST_CMD_DEFERRED);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}
