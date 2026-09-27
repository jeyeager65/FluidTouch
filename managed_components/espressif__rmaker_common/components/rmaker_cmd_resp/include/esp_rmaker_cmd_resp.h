/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
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

/* Super Admin User Flag*/
#define ESP_RMAKER_USER_ROLE_SUPER_ADMIN    (1 << 0)

/** Primary User Flag */
#define ESP_RMAKER_USER_ROLE_PRIMARY_USER   (1 << 1)

/** Secondary User Flag */
#define ESP_RMAKER_USER_ROLE_SECONDARY_USER (1 << 2)

/** Node User Flag
 *  This should be used when device initiates the command
 */
#define ESP_RMAKER_USER_ROLE_NODE           (1 << 3)

/** Mask to extract the base role (lower 4 bits) from the role byte */
#define ESP_RMAKER_USER_ROLE_MASK           (0x0F)

/** Mask to extract the sub-role (upper 4 bits) from the role byte */
#define ESP_RMAKER_USER_SUB_ROLE_MASK       (0xF0)

/** Get the base role from a role byte */
#define ESP_RMAKER_GET_USER_ROLE(role)      ((role) & ESP_RMAKER_USER_ROLE_MASK)

/** Get the sub-role number (1-4) from a role byte. Returns 0 if no sub-role bit is set. */
#define ESP_RMAKER_GET_USER_SUB_ROLE(role)  (__builtin_ffs(((role) & ESP_RMAKER_USER_SUB_ROLE_MASK) >> 4))


/** RainMaker Command Response TLV8 Types */
typedef enum {
    /** Request Id : Variable length string, max 32 characters*/
    ESP_RMAKER_TLV_TYPE_REQ_ID = 1,
    /** User Role : 1 byte */
    ESP_RMAKER_TLV_TYPE_USER_ROLE,
    /** Status : 1 byte */
    ESP_RMAKER_TLV_TYPE_STATUS,
    /** Timestamp : 4 bytes, little endian epoch */
    ESP_RMAKER_TLV_TYPE_TIMESTAMP,
    /** Command : 2 bytes*/
    ESP_RMAKER_TLV_TYPE_CMD,
    /** Data : Variable length */
    ESP_RMAKER_TLV_TYPE_DATA
} esp_rmaker_tlv_type_t;

/* RainMaker Command Response Status */
typedef enum {
    /** Success */
    ESP_RMAKER_CMD_STATUS_SUCCESS = 0,
    /** Generic Failure */
    ESP_RMAKER_CMD_STATUS_FAILED,
    /** Invalid Command */
    ESP_RMAKER_CMD_STATUS_CMD_INVALID,
    /** Authentication Failed */
    ESP_RMAKER_CMD_STATUS_AUTH_FAIL,
    /** Command not found */
    ESP_RMAKER_CMD_STATUS_NOT_FOUND,
    /** Last status value */
    ESP_RMAKER_CMD_STATUS_MAX,
} esp_rmaker_cmd_status_t;

#define REQ_ID_LEN  32
typedef struct {
    /** Command id */
    uint16_t cmd;
    /** Request id */
    char req_id[REQ_ID_LEN];
    /** User Role */
    uint8_t user_role;
    /** Timestamp (epoch seconds). 0 if not present in the command. */
    uint32_t timestamp;
} esp_rmaker_cmd_ctx_t;

typedef enum {
    /** Standard command: Set Parameters */
    ESP_RMAKER_CMD_TYPE_SET_PARAMS = 1,
    /** Last Standard command */
    ESP_RMAKER_CMD_STANDARD_LAST = 0xfff,
    /** Custom commands can start from here */
    ESP_RMAKER_CMD_CUSTOM_START = 0x1000
} esp_rmaker_cmd_t;

/** Command Response Handler
 *
 * If any command data is received from any of the supported transports (which are outside the scope of this core framework),
 * this function should be called to handle it and fill in the response.
 *
 * If the registered handler defers the response (see esp_rmaker_cmd_handler_t), this returns ESP_OK with
 * *output set to NULL. The caller should not publish anything in that case; the handler will later build
 * and publish the response via esp_rmaker_cmd_prepare_payload() and the transport's publish API.
 *
 * @param[in] input Pointer to input data.
 * @param[in] input_len data len.
 * @param[in] output Pointer to output data which should be set by the handler. Will be NULL if the handler deferred the response.
 * @param[out] output_len Length of output generated. Will be 0 if the handler deferred the response.
 *
 * @return ESP_OK on success (including the deferred case).
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_response_handler(const void *input, size_t input_len, void **output, size_t *output_len);

/** Prototype for Command Handler
 *
 * The handler to be invoked when a given command is received.
 *
 * For handlers that can complete synchronously, populate *out_data / *out_len and return ESP_OK.
 *
 * For handlers that need to defer the response (e.g. the command must be forwarded to another task or device
 * and the reply is needed to build the response), copy whatever is needed from in_data and ctx (both point to
 * framework-owned memory that is freed after this returns), queue the work, and return ESP_ERR_NOT_FINISHED.
 * When the async work completes, call esp_rmaker_cmd_prepare_payload() with the saved req_id and cmd (role=0,
 * status=STATUS_*, data=response) to assemble the TLV and hand the buffer to the transport's publish API.
 * Do not set *out_data when returning ESP_ERR_NOT_FINISHED.
 *
 * @param[in] in_data Pointer to input data.
 * @param[in] in_len data len.
 * @param[in] out_data Pointer to output data which should be set by the handler (ignored if returning ESP_ERR_NOT_FINISHED).
 * @param[out] out_len Length of output generated (ignored if returning ESP_ERR_NOT_FINISHED).
 * @param[in] ctx Command Context. Copy before returning if the response will be built asynchronously.
 * @param[in] priv Private data, if specified while registering command.
 *
 * @return ESP_OK on success (response in out_data / out_len).
 * @return ESP_ERR_NOT_FINISHED to defer the response; call esp_rmaker_cmd_prepare_payload() later.
 * @return Any other error to fail immediately with CMD_STATUS_FAILED.
 */
typedef esp_err_t (*esp_rmaker_cmd_handler_t)(const void *in_data, size_t in_len, void **out_data, size_t *out_len, esp_rmaker_cmd_ctx_t *ctx, void *priv);

/** Register a new command
 *
 * @param[in] cmd Command Identifier. Custom commands should start beyond ESP_RMAKER_CMD_STANDARD_LAST
 * @param[in] access User Access for the command. Can be an OR of the various user role flags like ESP_RMAKER_USER_ROLE_SUPER_ADMIN,
 * ESP_RMAKER_USER_ROLE_PRIMARY_USER and ESP_RMAKER_USER_ROLE_SECONDARY_USER
 * @param[in] handler The handler to be invoked when the given command is received.
 * @param[in] free_on_return Flag to indicate of the framework should free the output after it has been sent as response.
 * @param[in] priv Optional private data to be passed to the handler.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_register(uint16_t cmd, uint8_t access, esp_rmaker_cmd_handler_t handler, bool free_on_return, void *priv);

/** De-register a command
 *
 * @param[in] cmd Command Identifier. Custom commands should start beyond ESP_RMAKER_CMD_STANDARD_LAST
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_deregister(uint16_t cmd);

/* Prepare an empty command response
 *
 * This can be used to populate the request to be sent to get all pending commands
 *
 * @param[in] out_data Pointer to output data. This function will allocate memory and set this pointer
 * accordingly.
 * @param[out] out_len Length of output generated.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
 esp_err_t esp_rmaker_cmd_prepare_empty_response(void **output, size_t *output_len);

/** Prototype for Command sending function (TESTING only)
 *
 * @param[in] data Pointer to the data to be sent.
 * @param[in] data_len Size of data to be sent.
 * @param[in] priv Private data, if applicable.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
typedef esp_err_t (*esp_rmaker_cmd_send_t)(const void *data, size_t data_len, void *priv);

/** Send Test command (TESTING only)
 *
 * @param[in] req_id NULL terminated request id of max 32 characters.
 * @param[in] role User Role flag.
 * @param[in] cmd Command Identifier.
 * @param[in] data Pointer to data for the command.
 * @param[in] data_size Size of the data.
 * @param[in] cmd_send Transport specific function to send the command data.
 * @param[in] priv Private data (if any) to be sent to cmd_send.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_resp_test_send(const char *req_id, uint8_t role, uint16_t cmd, const void *data, size_t data_size, esp_rmaker_cmd_send_t cmd_send, void *priv);

/** Parse response (TESTING only)
 *
 * @param[in] response Pointer to the response received
 * @param[in] response_len Length of the response
 * @param[in] priv Private data, if any. Can be NULL.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_cmd_resp_parse_response(const void *response, size_t response_len, void *priv);

/** Prepare a TLV payload for a command or a response
 *
 * Single TLV builder used for both outgoing node-initiated commands (pass role, leave status = 0)
 * and responses to cloud-initiated commands (pass status, leave role = 0). The cloud ignores @p role
 * on responses and ignores @p status on node-initiated commands, so the same encoder serves both
 * directions.
 *
 * TLV emission rules:
 * - REQ_ID  : emitted iff @p req_id is non-NULL (a non-NULL empty string emits a zero-length REQ_ID,
 *             as used by esp_rmaker_cmd_prepare_empty_response()).
 * - USER_ROLE : emitted iff @p role is non-zero.
 * - STATUS  : always emitted.
 * - CMD     : always emitted (including cmd == 0).
 * - DATA    : emitted iff @p data is non-NULL and @p data_size > 0.
 *
 * @param[in] req_id      NULL terminated request id of max 32 characters (NULL to skip REQ_ID).
 * @param[in] role        User Role flag (0 to skip USER_ROLE).
 * @param[in] status      ESP_RMAKER_CMD_STATUS_* value for the STATUS TLV (ignored by the cloud
 *                        on node-initiated commands; pass 0 in that case).
 * @param[in] cmd         Command Identifier.
 * @param[in] data        Pointer to payload data (may be NULL).
 * @param[in] data_size   Size of @p data.
 * @param[out] output     Allocated TLV buffer. Caller frees.
 * @param[out] output_len Length of @p output.
 *
 * @return ESP_OK on success.
 * @return ESP_ERR_INVALID_ARG if @p output or @p output_len is NULL.
 * @return ESP_ERR_NO_MEM on allocation failure.
 */
esp_err_t esp_rmaker_cmd_prepare_payload(const char *req_id, uint8_t role, uint8_t status,
                                         uint16_t cmd,
                                         const void *data, size_t data_size,
                                         void **output, size_t *output_len);

/** @deprecated Use esp_rmaker_cmd_prepare_payload() instead. Retained as a one-release alias; will be
 *  removed in a future release. Expands to esp_rmaker_cmd_prepare_payload() with status = 0 (which the
 *  cloud ignores on node-initiated commands, matching the old behavior).
 */
#define esp_rmaker_cmd_resp_prepare_response_payload(req_id, role, cmd, data, data_size, output, output_len) \
    esp_rmaker_cmd_prepare_payload((req_id), (role), 0, (cmd), (data), (data_size), (output), (output_len))

#ifdef __cplusplus
}
#endif
