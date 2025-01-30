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

            case HTTP_EVENT_ON_HEADER:
                ESP_LOGI(LEDSTREAMER_HTTP_TAG, "HTTP_ON_HEADER: %s", evt->header_value);
            case HTTP_EVENT_ON_DATA:


                // ESP_LOGI(LEDSTREAMER_HTTP_TAG, "got %d bytes..", evt->data_len);
                uint16_t buffer_offset = 0;

                while (buffer_offset < evt->data_len)
                {
                    if (!qois_decodeBytes((uint8_t*)evt->data, evt->data_len, buffer_offset))
                    {

                        // ESP_LOGI(LEDSTREAMER_HTTP_TAG, "%lld", (qois_local_show_time-esp_timer_get_time()));
                        if (esp_timer_get_time() > qois_local_show_time)
                        {
                            //we're behind, correct a bit
                            // ESP_LOGI(QOISTAG,"Correcting +1mS");
                            qois_local_time_offset=qois_local_time_offset+1000;

                        }
                        else
                        {
                            while (esp_timer_get_time() < qois_local_show_time){};
                        }


                        leds_show();
                        qois_reset();

                    }
                }


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
