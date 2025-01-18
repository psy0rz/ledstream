//
// Created by psy on 1/18/25.
//

#ifndef LEDSTREA_HTTP_HPP
#define LEDSTREA_HTTP_HPP
#include "qois.hpp"

const char* LEDSTREAMER_HTTP_TAG = "ledstreamer_http";


inline void stream()
{
    ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connecting..");

    qois.reset();

    esp_http_client_config_t config = {
        .url = "http://192.168.13.154:3000/get/stream/panel1",
        .event_handler = [](esp_http_client_event_t* evt)
        {
            switch (evt->event_id)
            {
            default:
                break;
            case HTTP_EVENT_ON_DATA:

                int datai = 0;

                // ESP_LOGI(LEDSTREAMER_HTTP_TAG, "got %d bytes..", evt->data_len);

                while (datai < evt->data_len)
                {
                    if (!qois.decodeByte(((uint8_t*)evt->data)[datai]))
                    {
                        leds_show();
                        qois.reset();
                        // vTaskDelay(16 / portTICK_PERIOD_MS);
                    }

                    datai++;
                }


                break;
            }
            return ESP_OK;
        }
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
