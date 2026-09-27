/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define RMAKER_MQTT_QOS0    0
#define RMAKER_MQTT_QOS1    1

/** MQTT Connection parameters */
typedef struct {
    /** MQTT Host */
    char *mqtt_host;
    /** Client ID */
    char *client_id;
    /** Client Certificate in DER format or NULL-terminated PEM format */
    char *client_cert;
    /** Client Certificate length */
    size_t client_cert_len;
    /** Client Key in DER format or NULL-terminated PEM format */
    char *client_key;
    /** Client Key length */
    size_t client_key_len;
    /** Server Certificate in DER format or NULL-terminated PEM format */
    char *server_cert;
    /** Server Certificate length */
    size_t server_cert_len;
    /** Pointer for digital signature peripheral context */
    void *ds_data;
    /** Enable ECDSA peripheral, available in some Espressif devices.
     *
     * @note Supported only on ESP-IDF >= v5.5.1. On older versions, this field is
     * silently ignored and a warning is logged at runtime.
     */
    bool use_ecdsa_peripheral;
    /** ECDSA key block number from efuse, available in some Espressif devices.
     *
     * @note Supported only on ESP-IDF >= v5.5.1.
     */
    uint8_t ecdsa_key_efuse_blk;
    /** Last Will and Testament topic */
    char *mqtt_last_will_topic;
    /** Last Will and Testament message */
    char *mqtt_last_will_message;
    /** Last Will and Testament message length */
    size_t mqtt_last_will_message_len;
} esp_rmaker_mqtt_conn_params_t;

/** MQTT Get Connection Parameters function prototype
 *
 * @return Pointer to \ref esp_rmaker_mqtt_conn_params_t on success.
 * @return NULL on failure.
 */
typedef esp_rmaker_mqtt_conn_params_t *(*esp_rmaker_mqtt_get_conn_params_t)(void);

/** MQTT Subscribe callback prototype
 *
 * @param[in] topic Topic on which the message was received
 * @param[in] payload Data received in the message
 * @param[in] payload_len Length of the data
 * @param[in] priv_data The private data passed during subscription
 */
typedef void (*esp_rmaker_mqtt_subscribe_cb_t)(const char *topic, void *payload, size_t payload_len, void *priv_data);

/** MQTT Init function prototype
 *
 * @param[in] conn_params The MQTT connection parameters. If NULL is passed, it should internally use the
 * \ref esp_rmaker_mqtt_get_conn_params call if registered.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_init_t)(esp_rmaker_mqtt_conn_params_t *conn_params);

/** MQTT Deinit function prototype
 *
 * Call this function after MQTT has disconnected.
 */
typedef void (*esp_rmaker_mqtt_deinit_t)(void);

/** MQTT Connect function prototype
 *
 * Starts the connection attempts to the MQTT broker.
 * This should ideally be called after successful network connection.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_connect_t)(void);

/** MQTT Disconnect function prototype
 *
 * Disconnects from the MQTT broker.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_disconnect_t)(void);

/** MQTT Publish Message function prototype
 *
 * @param[in] topic The MQTT topic on which the message should be published.
 * @param[in] data Data to be published.
 * @param[in] data_len Length of the data.
 * @param[in] qos Quality of service for the message.
 * @param[out] msg_id If a non NULL pointer is passed, the id of the published message will be returned in this.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_publish_t)(const char *topic, void *data, size_t data_len, uint8_t qos, int *msg_id);

/** MQTT Subscribe function prototype
 *
 * @param[in] topic The topic to be subscribed to.
 * @param[in] cb The callback to be invoked when a message is received on the given topic.
 * @param[in] qos Quality of service for the subscription.
 * @param[in] priv_data Optional private data to be passed to the callback.
 *
 * @note With the default esp-mqtt glue, ESP_OK means the subscription was accepted, not that
 * the broker has acknowledged it. A request that cannot be sent, is rejected by the broker
 * (SUBACK failure code) or is dropped is retried internally with backoff, and all subscriptions
 * are re-sent on every reconnect. RMAKER_MQTT_EVENT_SUBSCRIBED and
 * RMAKER_MQTT_EVENT_SUBSCRIBE_FAILED (event data: the topic) report the outcome.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_subscribe_t)(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, uint8_t qos, void *priv_data);

/** MQTT Unsubscribe function prototype
 *
 * @param[in] topic Topic from which to unsubscribe.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_unsubscribe_t)(const char *topic);

/** MQTT Update Config function prototype
 *
 * Updates the MQTT configuration (including LWT) and reconnects.
 * This preserves subscriptions across the reconnection.
 *
 * @param[in] conn_params The new MQTT connection parameters.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
typedef esp_err_t (*esp_rmaker_mqtt_update_config_t)(esp_rmaker_mqtt_conn_params_t *conn_params);

/**  MQTT configuration */
typedef struct {
    /** Flag to indicate if the MQTT config setup is done */
    bool setup_done;
    /** Pointer to the Get MQTT params function. */
    esp_rmaker_mqtt_get_conn_params_t get_conn_params;
    /** Pointer to MQTT Init function. */
    esp_rmaker_mqtt_init_t init;
    /** Pointer to MQTT Deinit function. */
    esp_rmaker_mqtt_deinit_t deinit;
    /** Pointer to MQTT Connect function. */
    esp_rmaker_mqtt_connect_t connect;
    /** Pointer to MQTQ Disconnect function */
    esp_rmaker_mqtt_disconnect_t disconnect;
    /** Pointer to MQTT Publish function */
    esp_rmaker_mqtt_publish_t publish;
    /** Pointer to MQTT Subscribe function */
    esp_rmaker_mqtt_subscribe_t subscribe;
    /** Pointer to MQTT Unsubscribe function */
    esp_rmaker_mqtt_unsubscribe_t unsubscribe;
    /** Pointer to MQTT Update Config function */
    esp_rmaker_mqtt_update_config_t update_config;
} esp_rmaker_mqtt_config_t;

/** Setup MQTT Glue
 *
 * This function initializes MQTT glue layer with all the default functions.
 *
 * @param[out] mqtt_config Pointer to an allocated MQTT configuration structure.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_mqtt_glue_setup(esp_rmaker_mqtt_config_t *mqtt_config);

/* Get the ESP AWS PPI String
 *
 * @return pointer to a NULL terminated PPI string on success.
 * @return NULL in case of any error.
 */
const char *esp_get_aws_ppi(void);
#ifdef __cplusplus
}
#endif
