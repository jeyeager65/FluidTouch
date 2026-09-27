/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <stdint.h>
#include <esp_err.h>
#include <esp_event.h>
#ifdef __cplusplus
extern "C"
{
#endif

/** Initialize Factory NVS
 *
 * This initializes the Factory NVS partition which will store data
 * that should not be cleared even after a reset to factory.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_factory_init(void);

/** Get value from factory NVS
 *
 * This will search for the specified key in the Factory NVS partition,
 * allocate the required memory to hold it, copy the value and return
 * the pointer to it. It is responsibility of the caller to free the
 * memory when the value is no more required.
 *
 * @param[in] key The key of the value to be read from factory NVS.
 *
 * @return pointer to the value on success.
 * @return NULL on failure.
 */
void *esp_rmaker_factory_get(const char *key);

/** Get size of value from factory NVS
 *
 * This will search for the specified key in the Factory NVS partition,
 * and return the size of the value associated with the key.
 *
 * @param[in] key The key of the value to be read from factory NVS.
 *
 * @return size of the value on success.
 * @return 0 on failure.
 */
size_t esp_rmaker_factory_get_size(const char *key);

/** Set a value in factory NVS
 *
 * This will write the value for the specified key into factory NVS.
 *
 * @param[in] key The key for the value to be set in factory NVS.
 * @param[in] value Pointer to the value.
 * @param[in] len Length of the value.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_factory_set(const char *key, void *value, size_t len);
#ifdef __cplusplus
}
#endif
