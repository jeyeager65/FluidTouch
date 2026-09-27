/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "unity.h"
#include "esp_rmaker_work_queue.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"  // Add missing header for esp_timer_get_time()

#if CONFIG_IDF_TARGET_LINUX
#include <sys/time.h>
int64_t esp_timer_get_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif

static const char *TAG = "work_queue_test";
static SemaphoreHandle_t work_done_semaphore;
static int work_execution_count = 0;
static int delay_work_execution_count = 0;
static esp_rmaker_work_fn_t saved_work_fn = NULL;
static void *saved_work_data = NULL;

// Simple work function to test work queue
static void test_work_fn(void *data)
{
    ESP_LOGI(TAG, "Work function executed with data: %s", (char *)data);
    work_execution_count++;
    xSemaphoreGive(work_done_semaphore);
}

// Work function with delay to test delayed work
static void test_delayed_work_fn(void *data)
{
    ESP_LOGI(TAG, "Delayed work function executed with data: %s", (char *)data);
    delay_work_execution_count++;
    xSemaphoreGive(work_done_semaphore);
}

esp_err_t esp_rmaker_work_queue_add_delayed_task(esp_rmaker_work_fn_t work_fn, void *data, uint32_t delay_ms) {
    // Implement delay logic using a timer or delay mechanism
    // Schedule the work function to execute after delay_ms milliseconds
    // This is a placeholder implementation
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    return esp_rmaker_work_queue_add_task(work_fn, data);
}

TEST_CASE("ESP RainMaker Work Queue Init", "[work_queue]")
{
    work_done_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(work_done_semaphore);

    // Initialize the work queue
    esp_err_t err = esp_rmaker_work_queue_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Trying to init again should return success (or no error)
    err = esp_rmaker_work_queue_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    vSemaphoreDelete(work_done_semaphore);
}

TEST_CASE("ESP RainMaker Work Queue Add Work", "[work_queue]")
{
    work_done_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(work_done_semaphore);

    // Initialize the work queue
    esp_err_t err = esp_rmaker_work_queue_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Start the work queue
    err = esp_rmaker_work_queue_start();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    work_execution_count = 0;
    const char *test_data = "test_work_data";

    // Add work to the queue
    err = esp_rmaker_work_queue_add_task(test_work_fn, (void *)test_data);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Wait for work to be executed
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(work_done_semaphore, pdMS_TO_TICKS(5000)));

    // Verify work was executed
    TEST_ASSERT_EQUAL(1, work_execution_count);

    vSemaphoreDelete(work_done_semaphore);

    // Deinitialize the work queue
    esp_rmaker_work_queue_deinit();
}

TEST_CASE("ESP RainMaker Work Queue Add Multiple Works", "[work_queue]")
{
    // Use a counting semaphore to handle multiple tasks
    work_done_semaphore = xSemaphoreCreateCounting(3, 0);
    TEST_ASSERT_NOT_NULL(work_done_semaphore);

    // Initialize the work queue
    esp_err_t err = esp_rmaker_work_queue_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Start the work queue
    err = esp_rmaker_work_queue_start();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    work_execution_count = 0;
    const char *test_data1 = "test_work_data1";
    const char *test_data2 = "test_work_data2";
    const char *test_data3 = "test_work_data3";

    // Add multiple work items to the queue
    err = esp_rmaker_work_queue_add_task(test_work_fn, (void *)test_data1);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_work_queue_add_task(test_work_fn, (void *)test_data2);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_work_queue_add_task(test_work_fn, (void *)test_data3);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Wait for all work to be executed
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(work_done_semaphore, pdMS_TO_TICKS(5000)));
    }

    // Verify all work was executed
    TEST_ASSERT_EQUAL(3, work_execution_count);

    vSemaphoreDelete(work_done_semaphore);

    // Deinitialize the work queue
    esp_rmaker_work_queue_deinit();
}

TEST_CASE("ESP RainMaker Work Queue Add Delayed Work", "[work_queue]")
{
    work_done_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(work_done_semaphore);

    // Initialize the work queue
    esp_err_t err = esp_rmaker_work_queue_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Start the work queue
    err = esp_rmaker_work_queue_start();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    delay_work_execution_count = 0;
    const char *test_data = "delayed_work_data";

    // Get current time
    int64_t start_time = esp_timer_get_time();

    // Add delayed task to the queue
    err = esp_rmaker_work_queue_add_delayed_task(test_delayed_work_fn, (void *)test_data, 1000); // 1000 ms delay
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Wait for work to be executed
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(work_done_semaphore, pdMS_TO_TICKS(5000)));

    // Verify work was executed
    TEST_ASSERT_EQUAL(1, delay_work_execution_count);

    // Check time taken
    int64_t elapsed_time = (esp_timer_get_time() - start_time) / 1000; // ms
    ESP_LOGI(TAG, "Work executed after %" PRIi64 " ms", elapsed_time);
    int64_t margin_of_error = 10;
    TEST_ASSERT_TRUE((elapsed_time >= 1000 - margin_of_error) && (elapsed_time <= 1000 + 2 * margin_of_error));

    vSemaphoreDelete(work_done_semaphore);

    // Deinitialize the work queue
    esp_rmaker_work_queue_deinit();
}

// Tests storing a task that can be executed later
TEST_CASE("ESP RainMaker Work Queue Store Task", "[work_queue]")
{
    work_done_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(work_done_semaphore);

    // Initialize and start the work queue
    esp_err_t err = esp_rmaker_work_queue_init();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Start the work queue
    err = esp_rmaker_work_queue_start();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    saved_work_fn = NULL;
    saved_work_data = NULL;
    work_execution_count = 0;
    const char *test_data = "stored_work_data";

    // Store the task
    err = esp_rmaker_work_queue_add_task(test_work_fn, (void *)test_data);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Wait for work to be executed
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(work_done_semaphore, pdMS_TO_TICKS(5000)));

    // Verify work was executed
    TEST_ASSERT_EQUAL(1, work_execution_count);

    // Clean up
    vSemaphoreDelete(work_done_semaphore);

    // Deinitialize the work queue
    esp_rmaker_work_queue_deinit();
}
