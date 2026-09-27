#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "protocol_examples_common.h"
#include "string.h"
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esp_flash.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "soc/soc_caps.h"
#if SOC_RECOVERY_BOOTLOADER_SUPPORTED
#include "esp_efuse.h"
#endif
#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <sys/socket.h>
#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif

#include "partition_utils.h"
#include "esp_secure_cert_tlv_config.h"
#include "esp_secure_cert_read.h"
#include "esp_secure_cert_tlv_read.h"
#include "esp_secure_cert_signature_verify.h"
#include "esp_partition.h"
#ifdef CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
#include "esp_encrypted_img.h"
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
#define HMAC_UP_KEY_ID 2
#endif
#endif

#define ESP_SECURE_CERT_TLV_PARTITION_NAME      "esp_secure_cert"
#define ESP_SECURE_CERT_CUST_FLASH_PARTITION_TYPE           0x3F

// NVS namespace and keys for recovery
#define NVS_NAMESPACE_STAGING_PARTITION     "esc"
#define NVS_PARTITION_STAGING_ADDR          "stg_addr"
#define NVS_PARTITION_STAGING_SIZE          "stg_size"
#define NVS_PARTITION_STAGING_LABEL         "stg_label"
#define NVS_PARTITION_STAGING_TYPE          "stg_type"
#define NVS_PARTITION_STAGING_SUBTYPE       "stg_subtype"

#define MAX_OTA_URL_SIZE 256

static char TAG[] = "esp_secure_cert_ota_example";

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

static void read_custom_data()
{
    // Example: Read CA certificate using TLV API with type and subtype
    esp_secure_cert_tlv_config_t tlv_config = {
        .type = ESP_SECURE_CERT_USER_DATA_1,
        .subtype = ESP_SECURE_CERT_SUBTYPE_0
    };
    esp_secure_cert_tlv_info_t tlv_info = {0};

    esp_err_t err = esp_secure_cert_get_tlv_info(&tlv_config, &tlv_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read custom data (err=0x%x)", err);
    } else {
        ESP_LOGI(TAG, "Successfully read custom data (length=%lu)", tlv_info.length);
        ESP_LOGI(TAG, "Custom Data:\n%.*s", tlv_info.length, tlv_info.data);
        esp_secure_cert_free_tlv_info(&tlv_info);
    }
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_HEADERS_COMPLETE:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADERS_COMPLETE");
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t register_partition(size_t offset, size_t size, const char *label, esp_partition_type_t type, esp_partition_subtype_t subtype, const esp_partition_t **p_partition)
{
    // If the partition table contains this exact partition (matched by type, subtype AND label),
    // then reuse it; otherwise register it as an external partition.
    // Use esp_partition_find (iterator) instead of esp_partition_find_first so that we
    // look up an exact match and don't silently pick "any" partition that happens to
    // share the same type/subtype.
    *p_partition = NULL;
    esp_partition_iterator_t it = esp_partition_find(type, subtype, label);
    if (it != NULL) {
        *p_partition = esp_partition_get(it);
        esp_partition_iterator_release(it);
    }
    if ((*p_partition) == NULL) {
        esp_err_t error = esp_partition_register_external(NULL, offset, size, label, type, subtype, p_partition);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register %s partition (err=0x%x)", label, error);
            return error;
        }
        return ESP_OK;
    }
    ESP_LOGI(TAG, "Partition <%s> already registered at offset 0x%08" PRIx32, (*p_partition)->label, (*p_partition)->address);
    return ESP_OK;
}

#if !CONFIG_EXAMPLE_ESP_SECURE_CERT_DIRECT_OTA
/**
 * @brief Save staging partition info to NVS for recovery
 *
 * @param staging_partition Staging partition to save
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t save_staging_info_to_nvs(const esp_partition_t *staging_partition)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE_STAGING_PARTITION, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    // Save staging partition address
    err = nvs_set_u32(nvs_handle, NVS_PARTITION_STAGING_ADDR, staging_partition->address);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save staging address: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save staging partition size
    err = nvs_set_u32(nvs_handle, NVS_PARTITION_STAGING_SIZE, staging_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save staging size: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save staging partition label
    // Every field stored here is needed on the recovery path to rebuild the
    // staging partition; if any write fails we cannot recover reliably, so
    // treat all failures as fatal.
    err = nvs_set_str(nvs_handle, NVS_PARTITION_STAGING_LABEL, staging_partition->label);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save staging label: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save staging partition type
    err = nvs_set_u8(nvs_handle, NVS_PARTITION_STAGING_TYPE, staging_partition->type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save staging type: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Save staging partition subtype
    err = nvs_set_u8(nvs_handle, NVS_PARTITION_STAGING_SUBTYPE, staging_partition->subtype);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save staging subtype: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Staging partition info saved to NVS (addr=0x%08" PRIx32 ", size=%lu)",
                 staging_partition->address, staging_partition->size);
    } else {
        ESP_LOGE(TAG, "Failed to commit staging info to NVS: %s", esp_err_to_name(err));
    }

    return err;
}
#endif /* !CONFIG_EXAMPLE_ESP_SECURE_CERT_DIRECT_OTA */

/**
 * @brief Clear staging partition info from NVS
 *
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t clear_staging_info_from_nvs(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE_STAGING_PARTITION, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(err));
        return err;
    }

    // Erase all keys (optional, but cleaner)
    nvs_erase_key(nvs_handle, NVS_PARTITION_STAGING_ADDR);
    nvs_erase_key(nvs_handle, NVS_PARTITION_STAGING_SIZE);
    nvs_erase_key(nvs_handle, NVS_PARTITION_STAGING_LABEL);
    nvs_erase_key(nvs_handle, NVS_PARTITION_STAGING_TYPE);
    nvs_erase_key(nvs_handle, NVS_PARTITION_STAGING_SUBTYPE);

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Staging partition info cleared from NVS");
    }

    return err;
}

/**
 * @brief Check for recovery and complete pending OTA if needed
 *
 * This function checks NVS for recovery info. If found, it recreates
 * the staging partition, verifies its data, copies staging→primary,
 * and clears the recovery state.
 *
 * @return esp_err_t ESP_OK on success, or ESP_ERR_NOT_FOUND if no recovery needed
 */
static esp_err_t check_and_recover_staging_partition(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;
    uint32_t staging_addr = 0;
    uint32_t staging_size = 0;
    char staging_label[17] = {0}; // Max partition label length is 16
    uint8_t staging_type = 0;
    uint8_t staging_subtype = 0;

    err = nvs_open(NVS_NAMESPACE_STAGING_PARTITION, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        // Namespace doesn't exist - no recovery needed
        ESP_LOGD(TAG, "No recovery info found (namespace doesn't exist)");
        return ESP_ERR_NOT_FOUND;
    }

    // Read staging partition info
    err = nvs_get_u32(nvs_handle, NVS_PARTITION_STAGING_ADDR, &staging_addr);
    if (err != ESP_OK) {
        // Key doesn't exist - no recovery needed
        ESP_LOGD(TAG, "No recovery info found (staging address key missing)");
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    }

    err = nvs_get_u32(nvs_handle, NVS_PARTITION_STAGING_SIZE, &staging_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read staging size from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    // Read label (optional)
    size_t required_size = sizeof(staging_label);
    err = nvs_get_str(nvs_handle, NVS_PARTITION_STAGING_LABEL, staging_label, &required_size);
    if (err != ESP_OK) {
        // Label not critical, use default
        strncpy(staging_label, "recovery_staging", sizeof(staging_label) - 1);
        staging_label[sizeof(staging_label) - 1] = '\0'; // Ensure null termination
    }

    err = nvs_get_u8(nvs_handle, NVS_PARTITION_STAGING_TYPE, &staging_type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read staging type from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_get_u8(nvs_handle, NVS_PARTITION_STAGING_SUBTYPE, &staging_subtype);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read staging subtype from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);

    ESP_LOGW(TAG, "Recovery needed! Found staging partition info:");
    ESP_LOGW(TAG, "  Address: 0x%08" PRIx32, staging_addr);
    ESP_LOGW(TAG, "  Size: %lu bytes", staging_size);
    ESP_LOGW(TAG, "  Label: %s", staging_label);
    ESP_LOGW(TAG, "  Type: %u", staging_type);
    ESP_LOGW(TAG, "  Subtype: %u", staging_subtype);

    const esp_partition_t *recovered_partition = NULL;
    err = register_partition(staging_addr, staging_size, staging_label, staging_type, staging_subtype, &recovered_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register staging partition: %s", esp_err_to_name(err));
        return err;
    }

    // Set staging as active to verify its contents
    err = esp_secure_cert_tlv_set_partition(recovered_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set staging partition: %s", esp_err_to_name(err));
        return err;
    }

    // Re-verify the integrity of the staging partition before we proceed.
    // We are arriving here after a reboot, so the contents could have been
    // corrupted (e.g. by a power glitch) after the initial verify-and-save.
    // Refusing to copy corrupted data to the primary partition is critical
    // for the fail-safe property of this OTA flow.
    ESP_LOGI(TAG, "Verifying integrity of the staging partition before recovery copy");
    err = esp_secure_cert_verify_partition_integrity();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Staging partition integrity check failed during recovery: %s", esp_err_to_name(err));
        esp_secure_cert_tlv_set_partition(NULL);
        return err;
    }
    ESP_LOGI(TAG, "Staging partition integrity verified, reading data before recovery copy");
    read_custom_data();

    // Find the primary esp_secure_cert partition
    const esp_partition_t *primary_partition = esp_partition_find_first(
        ESP_SECURE_CERT_CUST_FLASH_PARTITION_TYPE, ESP_PARTITION_SUBTYPE_ANY,
        ESP_SECURE_CERT_TLV_PARTITION_NAME);
    if (primary_partition == NULL) {
        ESP_LOGE(TAG, "Primary esp_secure_cert partition not found for recovery");
        esp_secure_cert_tlv_set_partition(NULL);
        return ESP_ERR_NOT_FOUND;
    }

    // Copy staging → primary to complete the interrupted OTA
    ESP_LOGI(TAG, "Copying staging partition to primary esp_secure_cert partition");
    err = esp_partition_copy(primary_partition, 0, recovered_partition, 0, primary_partition->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to copy staging to primary partition: %s", esp_err_to_name(err));
        esp_secure_cert_tlv_set_partition(NULL);
        return err;
    }

    // Reset to primary partition
    esp_secure_cert_tlv_set_partition(NULL);
    ESP_LOGI(TAG, "Successfully copied staging data to primary partition");

    // Clear recovery info from NVS
    err = clear_staging_info_from_nvs();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to clear recovery info from NVS: %s", esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Recovery completed successfully");
    return ESP_OK;
}

#ifdef CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
static esp_err_t _decrypt_cb(decrypt_cb_arg_t *args, void *user_ctx)
{
    if (args == NULL || user_ctx == NULL) {
        ESP_LOGE(TAG, "_decrypt_cb: Invalid argument");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err;
    pre_enc_decrypt_arg_t pargs = {};
    pargs.data_in = args->data_in;
    pargs.data_in_len = args->data_in_len;
    err = esp_encrypted_img_decrypt_data((esp_decrypt_handle_t *)user_ctx, &pargs);
    if (err != ESP_OK && err != ESP_ERR_NOT_FINISHED) {
        ESP_LOGE(TAG, "Decrypt callback failed %d", err);
        free(pargs.data_out);
        return err;
    }

    if (pargs.data_out_len > 0) {
        args->data_out = pargs.data_out;
        args->data_out_len = pargs.data_out_len;
    } else {
        args->data_out_len = 0;
    }

    return ESP_OK;
}
#endif /* CONFIG_ESP_HTTPS_OTA_DECRYPT_CB */

/**
 * @brief Perform esp_secure_cert partition OTA update
 *
 * Similar to bootloader OTA, this function:
 * 1. Finds or creates a staging partition (unallocated space or passive OTA partition)
 * 2. Downloads the new esp_secure_cert data to staging partition
 * 3. Copies from staging to the primary esp_secure_cert partition
 * 4. Cleans up temporary partitions
 *
 * @param ota_config HTTPS OTA configuration
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t esp_secure_cert_ota_update(esp_https_ota_config_t *ota_config)
{
    const esp_partition_t *primary_esp_secure_cert;
    const esp_partition_t *staging_partition;
    esp_err_t err;

    // Find the primary esp_secure_cert partition
    primary_esp_secure_cert = esp_partition_find_first(ESP_SECURE_CERT_CUST_FLASH_PARTITION_TYPE, ESP_PARTITION_SUBTYPE_ANY, ESP_SECURE_CERT_TLV_PARTITION_NAME);
    if (primary_esp_secure_cert == NULL) {
        ESP_LOGE(TAG, "Primary esp_secure_cert partition not found");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Primary esp_secure_cert partition found at offset 0x%08" PRIx32 ", size %lu bytes",
             primary_esp_secure_cert->address, primary_esp_secure_cert->size);

#if CONFIG_EXAMPLE_ESP_SECURE_CERT_OTA_USE_UNALLOCATED_SPACE
    // Mode 1: Use unallocated space as staging area
    ESP_LOGI(TAG, "OTA Mode: Using unallocated space");

    uint32_t staging_offset;
    esp_err_t ret = partition_utils_find_unallocated(NULL, primary_esp_secure_cert->size,
                                                     ESP_PARTITION_TABLE_OFFSET + ESP_PARTITION_TABLE_SIZE,
                                                     &staging_offset, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to find unallocated space of size %lu bytes", primary_esp_secure_cert->size);
        return ret;
    }

    ESP_LOGI(TAG, "Found unallocated space at offset 0x%08" PRIx32, staging_offset);

    // Register the unallocated space as a temporary staging partition
    ret = register_partition(staging_offset, primary_esp_secure_cert->size, "StagingESC",
                           ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, &staging_partition);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register staging partition");
        return ret;
    }

#elif CONFIG_EXAMPLE_ESP_SECURE_CERT_USE_PASSIVE_OTA
    // Mode 2: Use passive OTA app partition as staging area
    ESP_LOGI(TAG, "OTA Mode: Using passive OTA partition");

    staging_partition = esp_ota_get_next_update_partition(NULL); // free app ota partition
    if (staging_partition == NULL) {
        ESP_LOGE(TAG, "No passive OTA partition available");
        return ESP_ERR_NOT_FOUND;
    }

#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    // Check if the passive OTA app partition is not needed for rollback
    esp_ota_img_states_t ota_state;
    esp_err_t ret = esp_ota_get_state_partition(staging_partition, &ota_state);
    if (ret == ESP_OK && ota_state == ESP_OTA_IMG_VALID) {
        ESP_LOGW(TAG, "Passive OTA app partition <%s> contains a valid app image eligible for rollback.",
                 staging_partition->label);
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    if (staging_partition->size < primary_esp_secure_cert->size) {
        ESP_LOGE(TAG, "Passive OTA partition too small: %lu < %lu bytes",
                 staging_partition->size, primary_esp_secure_cert->size);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Using passive OTA partition <%s> as staging area", staging_partition->label);

#elif CONFIG_EXAMPLE_ESP_SECURE_CERT_DIRECT_OTA
    // Mode 3: Direct OTA - write directly to primary partition (no staging)
    ESP_LOGW(TAG, "OTA Mode: Direct OTA (RISKY - NO STAGING)");
    ESP_LOGW(TAG, "WARNING: Any interruption will corrupt the esp_secure_cert partition!");
    ESP_LOGW(TAG, "Ensure stable power supply during the update!");

    staging_partition = primary_esp_secure_cert;

#else
    #error "No OTA mode selected"
#endif
    ota_config->partition.staging = staging_partition;
#if CONFIG_EXAMPLE_ESP_SECURE_CERT_DIRECT_OTA
    ota_config->partition.final = NULL;
#else
    ota_config->partition.final = primary_esp_secure_cert;
#endif
    ESP_LOGI(TAG, "Starting OTA download to staging partition <%s>...", staging_partition->label);
    err = esp_https_ota(ota_config);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTPS OTA failed (err=0x%x)", err);

        // Cleanup: deregister temporary partition if it was created
#if CONFIG_EXAMPLE_ESP_SECURE_CERT_OTA_USE_UNALLOCATED_SPACE
        esp_partition_deregister_external(staging_partition);
#elif CONFIG_EXAMPLE_ESP_SECURE_CERT_USE_PASSIVE_OTA && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
        if (staging_partition != esp_ota_get_next_update_partition(NULL)) {
            esp_partition_deregister_external(staging_partition);
        }
#endif
        return err;
    }

    ESP_LOGI(TAG, "OTA download completed successfully");
    ESP_LOGI(TAG, "Verifying integrity of the staging partition after downloading");
    esp_secure_cert_tlv_set_partition(staging_partition); /* Set the staging partition as the active partition for secure verification   */
    err = esp_secure_cert_verify_partition_integrity();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to verify integrity of the staging partition after downloading: %s", esp_err_to_name(err));
        esp_secure_cert_tlv_set_partition(NULL); /* Reset to the original partition on failure */
        return err;
    }
    ESP_LOGI(TAG, "Partition integrity verified successfully");

#if !CONFIG_EXAMPLE_ESP_SECURE_CERT_DIRECT_OTA
#if CONFIG_ESP_SECURE_CERT_SECURE_VERIFICATION
    err = esp_secure_cert_verify_partition_signature(NULL); /* Verify the signature of the esp_secure_cert partition */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to verify esp_secure_cert partition signature: %s", esp_err_to_name(err));
        esp_secure_cert_tlv_set_partition(NULL); /* Reset to the original partition on failure */
        return err;
    }
#endif /* CONFIG_ESP_SECURE_CERT_SECURE_VERIFICATION */
    /* Save staging partition info to NVS for recovery*/
    ESP_LOGI(TAG, "Saving staging partition info to NVS for recovery");
    err = save_staging_info_to_nvs(staging_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save staging partition info to NVS for recovery (err=0x%x)", err);
    } else {
        ESP_LOGI(TAG, "Successfully saved staging partition info to NVS for recovery");
    }

    /* Check if we can read the ca cert from the staging partition*/
    ESP_LOGI(TAG, "Checking CA certificate in the staging partition");
    read_custom_data();
    ESP_LOGW(TAG, "Ensure stable power supply! Loss of power at this stage may corrupt esp_secure_cert partition");
    ESP_LOGI(TAG, "Copying from <%s> staging partition to <%s>...",
             staging_partition->label, primary_esp_secure_cert->label);
    err = esp_partition_copy(primary_esp_secure_cert, 0, staging_partition, 0, primary_esp_secure_cert->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to copy partition to primary esp_secure_cert (err=0x%x). Partition may be corrupted!", err);
        // Cleanup: deregister temporary partition
#if CONFIG_EXAMPLE_ESP_SECURE_CERT_OTA_USE_UNALLOCATED_SPACE
        esp_partition_deregister_external(staging_partition);
#elif CONFIG_EXAMPLE_ESP_SECURE_CERT_USE_PASSIVE_OTA && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
        if (staging_partition != esp_ota_get_next_update_partition(NULL)) {
            esp_partition_deregister_external(staging_partition);
        }
#endif
        return err;
    }
    ESP_LOGI(TAG, "Successfully copied to primary esp_secure_cert partition");

    esp_secure_cert_tlv_set_partition(NULL);
    /* Check if we can read the ca cert from the primary partition*/
    ESP_LOGI(TAG, "Checking CA certificate in the primary partition after copying");
    read_custom_data();
    // Clear recovery info from NVS after successful copy
    err = clear_staging_info_from_nvs();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to clear recovery info from NVS: %s", esp_err_to_name(err));
    }
    // Cleanup: deregister temporary partition
#if CONFIG_EXAMPLE_ESP_SECURE_CERT_OTA_USE_UNALLOCATED_SPACE
    esp_partition_deregister_external(staging_partition);
    ESP_LOGI(TAG, "Deregistered temporary staging partition");
#elif CONFIG_EXAMPLE_ESP_SECURE_CERT_USE_PASSIVE_OTA && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    if (staging_partition != esp_ota_get_next_update_partition(NULL)) {
        esp_partition_deregister_external(staging_partition);
        ESP_LOGI(TAG, "Deregistered temporary staging partition");
    }
#endif /* CONFIG_EXAMPLE_ESP_SECURE_CERT_OTA_USE_UNALLOCATED_SPACE */
#endif /* CONFIG_EXAMPLE_ESP_SECURE_CERT_DIRECT_OTA */
    ESP_LOGI(TAG, "ESP Secure Cert OTA update completed successfully");
    return ESP_OK;
}

static void esp_secure_cert_ota_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting ESP Secure Cert OTA task");
    esp_http_client_config_t config = {
        .url = CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL,
        .event_handler = _http_event_handler,
        .cert_pem = (const char *)server_cert_pem_start,
        .keep_alive_enable = true,
        .timeout_ms = CONFIG_EXAMPLE_OTA_RECV_TIMEOUT,
        .skip_cert_common_name_check = true,
    };

#ifdef CONFIG_EXAMPLE_CERT_OTA_URL_FROM_STDIN
    char url_buf[MAX_OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, MAX_OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
    // Initialize decrypt configuration and handle for pre-encrypted OTA
    esp_decrypt_cfg_t decrypt_cfg = {0};
#if defined(CONFIG_PRE_ENCRYPTED_OTA_USE_RSA)
#if defined(CONFIG_PRE_ENCRYPTED_RSA_USE_DS)
    esp_ds_data_ctx_t *ds_data = esp_secure_cert_get_ds_ctx();
    if (ds_data == NULL) {
        ESP_LOGE(TAG, "Failed to get DS context");
        vTaskDelete(NULL);
        return;
    }
    decrypt_cfg.ds_data = ds_data;
#else
    extern const char rsa_private_pem_start[] asm("_binary_private_pem_start");
    extern const char rsa_private_pem_end[] asm("_binary_private_pem_end");
    decrypt_cfg.rsa_priv_key = rsa_private_pem_start;
    decrypt_cfg.rsa_priv_key_len = rsa_private_pem_end - rsa_private_pem_start;
#endif /* CONFIG_PRE_ENCRYPTED_RSA_USE_DS */
#elif defined(CONFIG_PRE_ENCRYPTED_OTA_USE_ECIES)
    decrypt_cfg.hmac_key_id = HMAC_UP_KEY_ID;
#endif /* CONFIG_PRE_ENCRYPTED_OTA_USE_RSA */

    esp_decrypt_handle_t decrypt_handle = esp_encrypted_img_decrypt_start(&decrypt_cfg);
    if (!decrypt_handle) {
        ESP_LOGE(TAG, "Failed to initialize decrypt handle for pre-encrypted OTA");
        vTaskDelete(NULL);
        return;
    }
#endif /* CONFIG_ESP_HTTPS_OTA_DECRYPT_CB */

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
#ifdef CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
        .decrypt_cb = _decrypt_cb,
        .decrypt_user_ctx = (void *)decrypt_handle,
        .enc_img_header_size = esp_encrypted_img_get_header_size(),
#endif
    };

    esp_err_t ret = esp_secure_cert_ota_update(&ota_config);

#ifdef CONFIG_ESP_HTTPS_OTA_DECRYPT_CB
    // Cleanup decrypt handle - check if data was completely received first
    if (ret != ESP_OK) {
        esp_encrypted_img_decrypt_abort(decrypt_handle);
    } else {
        // Check if all data was decrypted before calling decrypt_end
        if (esp_encrypted_img_is_complete_data_received(decrypt_handle)) {
            esp_err_t decrypt_end_err = esp_encrypted_img_decrypt_end(decrypt_handle);
            if (decrypt_end_err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to finalize decrypt handle: %s", esp_err_to_name(decrypt_end_err));
            }
        } else {
            ESP_LOGW(TAG, "Not all data was decrypted, aborting decrypt handle");
            esp_encrypted_img_decrypt_abort(decrypt_handle);
        }
    }
#endif

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ESP Secure Cert OTA succeeded!");
        ESP_LOGI(TAG, "You can now restart to use the new certificate data");
        // Note: Unlike firmware OTA, we don't automatically restart
        // The application can continue running with old cert until manual restart
    } else {
        ESP_LOGE(TAG, "ESP Secure Cert OTA failed (err=0x%x)", ret);
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "OTA example app_main start");
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

#if CONFIG_EXAMPLE_CONNECT_WIFI
    /* Ensure to disable any WiFi power save mode, this allows best throughput
     * and hence timings for overall OTA operation.
     */
    esp_wifi_set_ps(WIFI_PS_NONE);
#endif // CONFIG_EXAMPLE_CONNECT_WIFI

    err = check_and_recover_staging_partition();
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "========================================");
        ESP_LOGW(TAG, "RECOVERY: Previous OTA was interrupted");
        ESP_LOGW(TAG, "Staging data has been copied to primary partition");
        ESP_LOGW(TAG, "========================================");
        ESP_LOGI(TAG, "Verifying CA certificate in primary partition after recovery");
        read_custom_data();
    } else if (err == ESP_ERR_NOT_FOUND) {
        xTaskCreate(&esp_secure_cert_ota_task, "esp_secure_cert_ota_task", 8192, NULL, 5, NULL);
    } else {
        ESP_LOGE(TAG, "Error while recovering the partition");
    }

    ESP_LOGI(TAG, "Returned from the app main");
}
