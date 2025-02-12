//connect to ledder via http and stream led animtion via qois. can also write animation to flash.


#ifndef LEDSTREA_HTTP_HPP
#define LEDSTREA_HTTP_HPP

#include <fileserver.hpp>
// #include <ledstreamer_flash.hpp>

#include <ledstreamer_flash.hpp>

#include "qois.hpp"

const char* LEDSTREAMER_HTTP_TAG = "ledstreamer_http";

char url[200];

fileserver_ctx* ledstreamer_http_file_ctx = nullptr;

bool stream_flashing = false;
bool stream_live=false;

inline void IRAM_ATTR stream()
{
    ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connecting: %s", url);

    stream_flashing = false;
    stream_live = false;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = [](esp_http_client_event_t* evt)

        {
            switch (evt->event_id)
            {
            default:
                break;


            case HTTP_EVENT_ON_CONNECTED:
                ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connected");
                break;

            case HTTP_EVENT_ON_HEADER:
                if (strcmp(evt->header_key, "Mode") == 0)
                {
                    //live streaming
                    if (strcmp(evt->header_value, "0") == 0)
                    {
                        ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Live streaming");
                        vTaskDelay(1000 / portTICK_PERIOD_MS);
                        ledstreamer_flash_stop();
                        qois_reset();
                        timing_reset();
                        stream_live=true;

                    }
                    //record
                    else if (strcmp(evt->header_value, "1") == 0)
                    {
                        stream_flashing = true;
                        stream_live=true;
                        ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Recording");
                        ledstreamer_flash_stop();
                        qois_reset();
                        timing_reset();
                    }
                    //play
                    else if (strcmp(evt->header_value, "2") == 0)
                    {
                        ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Replaying");
                        ledstreamer_flash_start();
                    }
                    else
                    {
                            ESP_LOGE(LEDSTREAMER_HTTP_TAG, "Unknown mode");
                    }
                }


                break;

            case HTTP_EVENT_ON_DATA:

                //TODO: writing in seperate thread?
                if (stream_flashing)
                {
                    if (ledstreamer_http_file_ctx == nullptr)
                        ledstreamer_http_file_ctx = fileserver_open(true);
                    fileserver_write(ledstreamer_http_file_ctx, evt->data, evt->data_len);
                }

                if (stream_live)
                    qois_decodeBytes(static_cast<uint8_t*>(evt->data), evt->data_len, 0);


                break;
            }
            return ESP_OK;
        },

    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_perform(client) == ESP_OK)
        ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Stream ended");
    else
        ESP_LOGE(LEDSTREAMER_HTTP_TAG, "Streaming failed");


    esp_http_client_cleanup(client);
    fileserver_close(ledstreamer_http_file_ctx);

    ledstreamer_flash_start();

}


[[noreturn]] inline void ledstreamer_http_task(void* args)

{
    int64_t lastAttempt = 0;
    while (true)
    {
        lastAttempt = esp_timer_get_time();
        stream();
        if (abs(esp_timer_get_time() - lastAttempt) < 1000000)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}


inline void ledstreamer_http_init()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA); // Get the MAC address

    // Format the MAC address as a string
    snprintf(url, sizeof(url), "%s/%02X%02X%02X%02X%02X%02X",
             CONFIG_LEDSTREAM_LEDDER_URL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    xTaskCreate(ledstreamer_http_task, "ledstreamer_http_task", 4096, nullptr, 10, nullptr);
}


#endif //LEDSTREA_HTTP_HPP
