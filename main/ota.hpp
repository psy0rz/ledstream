#include <string.h>
#include <utils.hpp>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include  "leds.hpp"

#ifndef OTA_UPDATER_HEADER
#define OTA_UPDATER_HEADER

class OTAUpdater {
public:
    OTAUpdater()  {
        updating = false;
    }


    void check_update() {
        if (updating) {
            ESP_LOGI(TAG, "Update already in progress");
            return;
        }

        updating = true;
//        xTaskCreate(ota_update_task, "ota_update_task", 8192, this, 5, &ota_task_handle);
        xTaskCreate(&ota_task, "ota_task", 8192, this, 5, NULL);
    }

private:
    static const char *TAG;

    bool updating;


    int total_length = 0;
    int downloaded_length = 0;
    int percentage = 0;

    static void ota_task(void *pvParameter) {
        OTAUpdater *instance = static_cast<OTAUpdater *>(pvParameter);
        instance->update_firmware();
        instance->updating = false;
        vTaskDelete(NULL);
    }

    bool is_update_needed(const esp_app_desc_t *new_app_info) {
        const esp_app_desc_t *current_desc = esp_ota_get_app_description();
        ESP_LOGI(TAG, "Current firmware  : %s %s", current_desc->date, current_desc->time);
        ESP_LOGI(TAG, "Available firmware: %s %s", new_app_info->date, new_app_info->time);
        return (
                strncmp(new_app_info->date, current_desc->date, sizeof(new_app_info->date)) != 0 ||
                strncmp(new_app_info->time, current_desc->time, sizeof(new_app_info->time)) != 0);
    }


    void update_firmware() {

        ESP_LOGI(TAG, "Checking for updates at %s ...", CONFIG_LEDSTREAM_FIRMWARE_UPGRADE_URL);

        esp_err_t err;
        esp_http_client_config_t config = {};
        config.url = CONFIG_LEDSTREAM_FIRMWARE_UPGRADE_URL;
//        config.cert_pem = server_cert_pem;

        esp_https_ota_config_t ota_config = {};
        ota_config.http_config = &config;

        esp_https_ota_handle_t ota_handle = NULL;
        err = esp_https_ota_begin(&ota_config, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_https_ota_begin failed");
            return;
        }

        esp_app_desc_t new_app_info;
        err = esp_https_ota_get_img_desc(ota_handle, &new_app_info);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get the image description");
            esp_https_ota_abort(ota_handle);
            return;
        }

        if (!is_update_needed(&new_app_info)) {
            ESP_LOGI(TAG, "No update available");
            esp_https_ota_abort(ota_handle);
            return;
        }

        ESP_LOGW(TAG, "Firmware update started...");

        int binary_file_length = esp_https_ota_get_image_size(ota_handle);

        while (1) {
            esp_err_t ota_result = esp_https_ota_perform(ota_handle);
            if (ota_result != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
                break;
            }

            int progress = (esp_https_ota_get_image_len_read(ota_handle) * 100) / binary_file_length;
            progress_bar(progress);
        }

        err = esp_https_ota_finish(ota_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "OTA update completed. Restarting...");
            esp_restart();
        } else {
            ESP_LOGE(TAG, "OTA update failed");
        }
    }


};

const char *OTAUpdater::TAG = "ota";

extern OTAUpdater ota_updater;

#endif
