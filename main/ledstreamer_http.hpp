//connect to ledder via http and stream led animtion via qois. can also write animation to flash.


#ifndef LEDSTREA_HTTP_HPP
#define LEDSTREA_HTTP_HPP

#include <fileserver.hpp>
// #include <ledstreamer_flash.hpp>

#include "qois.hpp"

const char* LEDSTREAMER_HTTP_TAG = "ledstreamer_http";

char url[200];

bool stream_flashing=false;
fileserver_ctx *stream_ctx= nullptr;


inline void IRAM_ATTR stream()
{
    ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connecting: %s", url);


    qois_reset();

    stream_flashing=false;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = [](esp_http_client_event_t* evt)

        {
            switch (evt->event_id)
            {
            default:
                break;


            case HTTP_EVENT_ON_HEADER:
                if (strcmp(evt->header_key, "Flash")==0)
                {
                    if (strcmp(evt->header_value,"1")==0)
                    {
                        stream_flashing=true;
                    }
                }

                break;

            case HTTP_EVENT_ON_DATA:

                if (stream_flashing)
                {
                    if (stream_ctx==nullptr)
                        stream_ctx=fileserver_open(true);
                    fileserver_write(stream_ctx, evt->data, evt->data_len);
                }

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
    fileserver_close(stream_ctx);
}


[[noreturn]] inline void ledstreamer_http_task(void* args)

{
    while (true)
    {
        stream();
        // ledstreamer_flash_start();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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
