//
// Created by psy on 1/18/25.
//

#ifndef LEDSTREA_HTTP_HPP
#define LEDSTREA_HTTP_HPP

#include "qois.hpp"

const char* LEDSTREAMER_HTTP_TAG = "ledstreamer_http";


inline void IRAM_ATTR stream()
{
    ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connecting: %s", CONFIG_LEDSTREAM_LEDDER_URL);

    qois_reset();


    esp_http_client_config_t config = {
        .url = CONFIG_LEDSTREAM_LEDDER_URL,
        .event_handler = [](esp_http_client_event_t* evt)

        {
            switch (evt->event_id)
            {
            default:
                break;

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

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}


[[noreturn]] inline void ledstreamer_http_task(void* args)

{
    while (true)
        stream();
}


#endif //LEDSTREA_HTTP_HPP
