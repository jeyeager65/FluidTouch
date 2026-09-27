/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <esp_err.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Reboot the device after a delay
 *
 * This API just starts a reboot timer and returns immediately.
 * The actual reboot is trigerred asynchronously in the timer callback.
 * This is useful if you want to reboot after a delay, to allow other tasks to finish
 * their operations (Eg. MQTT publish to indicate OTA success). The \ref RMAKER_EVENT_REBOOT
 * event is triggered when the reboot timer is started.
 *
 * @param[in] seconds Time in seconds after which the device should reboot.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_reboot(int8_t seconds);

/** Reset Wi-Fi credentials and (optionally) reboot
 *
 * This will reset just the Wi-Fi credentials and (optionally) trigger a reboot.
 * This is useful when you want to keep all the entries in NVS memory
 * intact, but just change the Wi-Fi credentials. The \ref RMAKER_EVENT_WIFI_RESET
 * event is triggered when this API is called. The actual reset will happen after a
 * delay if reset_seconds is not zero.
 *
 * @note This reset and reboot operations will happen asynchronously depending
 * on the values passed to the API.
 *
 * @param[in] reset_seconds Time in seconds after which the reset should get triggered.
 * This will help other modules take some actions before the device actually resets.
 * If set to zero, the operation would be performed immediately.
 * @param[in] reboot_seconds Time in seconds after which the device should reboot. If set
 * to negative value, the device will not reboot at all.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_wifi_reset(int8_t reset_seconds, int8_t reboot_seconds);

/** Reset to factory defaults and reboot
 *
 * This will clear entire NVS partition and (optionally) trigger a reboot.
 * The \ref RMAKER_EVENT_FACTORY_RESET event is triggered when this API is called.
 * The actual reset will happen after a delay if reset_seconds is not zero.
 *
 * @note This reset and reboot operations will happen asynchronously depending
 * on the values passed to the API.
 *
 * @param[in] reset_seconds Time in seconds after which the reset should get triggered.
 * This will help other modules take some actions before the device actually resets.
 * If set to zero, the operation would be performed immediately.
 * @param[in] reboot_seconds Time in seconds after which the device should reboot. If set
 * to negative value, the device will not reboot at all.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_factory_reset(int8_t reset_seconds, int8_t reboot_seconds);

#ifdef __cplusplus
}
#endif
