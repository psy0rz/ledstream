#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "FastLED.h"

#ifndef OTA_UPDATER_HEADER
#define OTA_UPDATER_HEADER

class OTAUpdater {
public:
    OTAUpdater(CRGB *leds) : leds(leds) {
        ota_semaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(ota_semaphore);
    }

    void begin() {
        xTaskCreate(&ota_task, "ota_task", 8192, this, 5, NULL);
    }

    void check_updates() {
        if (xSemaphoreTake(ota_semaphore, (TickType_t) 10) == pdTRUE) {
            begin();
        } else {
            ESP_LOGW(TAG, "OTA task already running, skipping check_updates.");
        }
    }

private:
    static const char *TAG;

    CRGB *leds;
    SemaphoreHandle_t ota_semaphore;

    int total_length = 0;
    int downloaded_length = 0;
    int percentage = 0;

    static void ota_task(void *pvParameter) {
        OTAUpdater *instance = static_cast<OTAUpdater *>(pvParameter);
        instance->perform_update();
        vTaskDelete(NULL);
    }

    void perform_update() {
        ESP_LOGI(TAG, "Starting OTA...");

        esp_http_client_config_t config = {};

        config.url = CONFIG_LEDSTREAM_FIRMWARE_UPGRADE_URL;
        config.timeout_ms = 5000;
        config.event_handler = http_client_event_handler;
        config.user_data = this;

        total_length = 0;
        downloaded_length = 0;
        percentage = 0;


        esp_err_t ret = esp_https_ota(&config);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA update succeeded. Rebooting...");
            esp_restart();
        } else {
            ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(ret));
        }

        xSemaphoreGive(ota_semaphore);
    }

    static esp_err_t http_client_event_handler(esp_http_client_event_t *evt) {
        OTAUpdater *instance = static_cast<OTAUpdater *>(evt->user_data);
int percentage=0;


        switch (evt->event_id) {
            case HTTP_EVENT_ERROR:
                ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
                break;
            case HTTP_EVENT_ON_HEADER:
                if (strcasecmp(evt->header_key, "Content-Length") == 0) {
                    instance->total_length = atoi(evt->header_value);
                    ESP_LOGI(TAG, "Content-Length: %d", instance->total_length);
                }
                break;
            case HTTP_EVENT_ON_DATA:
                instance->downloaded_length += evt->data_len;
                percentage = (instance->downloaded_length * 100) / instance->total_length;

                if (percentage != instance->percentage) {
                    ESP_LOGI(TAG, "Progress: %d%%", percentage);
                    instance->percentage=percentage;
                    instance->update_progress_bar(percentage);
                }

                break;
            default:
                break;
        }
        return ESP_OK;
    }

    void update_progress_bar(int percentage) {
        const int NUM_LEDS = 8;
        int num_leds_lit = ((percentage * NUM_LEDS ) / 100);
//        static int flash_state = 0;

        for (int i = 0; i < NUM_LEDS; i++) {
            if (i < num_leds_lit) {
                leds[i] = CRGB::DarkGreen;
            } else {
                leds[i] = CRGB::DarkRed;
            }
        }
        FastLED.show();
    }
};

const char *OTAUpdater::TAG = "ota";

extern OTAUpdater ota_updater;

#endif
