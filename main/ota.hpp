/* Advanced HTTPS OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"


static const char *OTA_TAG = "ota";
//extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
//extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");


//compare image headers and check if we need to update
static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(OTA_TAG, "Running firmware version: %s", running_app_info.version);
    }

//    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
    if (new_app_info->time==running_app_info.time && new_app_info->date==running_app_info.date) {
        ESP_LOGI(OTA_TAG, "No update needed.");
        return ESP_FAIL;
    }

#ifdef CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK
    /**
     * Secure version check from firmware image header prevents subsequent download and flash write of
     * entire firmware image. However this is optional because it is also taken care in API
     * esp_https_ota_finish at the end of OTA update procedure.
     */
    const uint32_t hw_sec_version = esp_efuse_read_secure_version();
    if (new_app_info->secure_version < hw_sec_version) {
        ESP_LOGW(OTA_TAG, "New firmware security version is less than eFuse programmed, %d < %d", new_app_info->secure_version, hw_sec_version);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

static esp_err_t _http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

//called each time wifi/network is connected
void ota_stuff(void)
{

    int total_size=0;
    int last_progress=-1;

    /**
     * We are treating successful WiFi connection as a checkpoint to cancel rollback
     * process and mark newly updated firmware image as active. For production cases,
     * please tune the checkpoint behavior per end application requirement.
     */
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
                ESP_LOGI(OTA_TAG, "App is valid, rollback cancelled successfully");
            } else {
                ESP_LOGE(OTA_TAG, "Failed to cancel rollback");
            }
        }
    }


    ESP_LOGI(OTA_TAG, "Starting Advanced OTA example");

    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = CONFIG_LEDSTREAM_FIRMWARE_UPGRADE_URL,
//        .cert_pem = (char *)server_cert_pem_start,
        .timeout_ms = 5000,
        .keep_alive_enable = true,
    };


//    config.skip_cert_common_name_check = true;

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = _http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
        .partial_http_download = false,
//        .max_http_request_size = 64000,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "ESP HTTPS OTA Begin failed");
//        vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(OTA_TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        //no update needed
        goto ota_end;
    }

    total_size=esp_https_ota_get_image_size(https_ota_handle);

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        int read_size=esp_https_ota_get_image_len_read(https_ota_handle);
//        ESP_LOGI(OTA_TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
        int progress=read_size*100/total_size;
        if (progress != last_progress) {
            ESP_LOGI(OTA_TAG, "Progress %d %%", progress);
            last_progress=progress;
        }

    }
            ESP_LOGI(OTA_TAG, "TAK");

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(OTA_TAG, "Complete data was not received.");
    } else {
            ESP_LOGI(OTA_TAG, "finishinggg");
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
            ESP_LOGI(OTA_TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(OTA_TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(OTA_TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
//            vTaskDelete(NULL);
        }
    }

ota_end:
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(OTA_TAG, "ESP_HTTPS_OTA upgrade failed");
//    vTaskDelete(NULL);

}
