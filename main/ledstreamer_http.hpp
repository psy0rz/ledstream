//
// Created by psy on 1/18/25.
//

#ifndef LEDSTREA_HTTP_HPP
#define LEDSTREA_HTTP_HPP

#include "qois.hpp"

const char* LEDSTREAMER_HTTP_TAG = "ledstreamer_http";

char url[200];


inline void IRAM_ATTR stream()
{
    ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connecting: %s", url);

    qois_reset();


    esp_http_client_config_t config = {
        .url = url,
        .event_handler = [](esp_http_client_event_t* evt)

        {
            switch (evt->event_id)
            {
            default:
                break;


            // case HTTP_EVENT_ON_HEADER:
            //     ESP_LOGI(LEDSTREAMER_HTTP_TAG, "HTTP_ON_HEADER: %s", evt->header_key);
            //     if (evt->header_key=="Flash")
            //     {
            //         if (evt->header_key=="1")
            //         {
            //
            //         }
            //     }
            //
            //     break;

            case HTTP_EVENT_ON_DATA:


                qois_decodeBytes((uint8_t*)evt->data, evt->data_len, 0);


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
}


[[noreturn]] inline void ledstreamer_http_task(void* args)

{
    while (true)
    {
        stream();
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

    xTaskCreate(ledstreamer_http_task, "ledstreamer_http_task", 4096, nullptr, 1, nullptr);

}


#endif //LEDSTREA_HTTP_HPP
