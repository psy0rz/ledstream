
#ifndef LEDSTREAM_STATUS_LED_HPP
#define LEDSTREAM_STATUS_LED_HPP

#include "driver/gpio.h"
#include "wifi.hpp"
#include "ledstreamer_http.hpp"

//blink while disconnected, solid on once we have an IP
static void status_led_task(void *pvParameter) {
    const gpio_num_t pin = (gpio_num_t) CONFIG_LEDSTREAM_STATUS_LED_PIN;
    gpio_reset_pin(pin);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);

    while(1) {
        //all ok heartbeat
        int on_time=100;
        int off_time=1900;

        if (wifi_disconnected)
        {
            //no wifi panic
            on_time=100;
            off_time=100;
        }
        else if (!http_connected)
        {
            //connecting http blinker
            on_time=1000;
            off_time=1000;
        }

        gpio_set_level(pin, false);
        vTaskDelay(pdMS_TO_TICKS(on_time));

        gpio_set_level(pin, true);
        vTaskDelay(pdMS_TO_TICKS(off_time));


    }
}


void status_led_init()
{
    #if CONFIG_LEDSTREAM_STATUS_LED_PIN >= 0
        xTaskCreate(&status_led_task, "wifi_status_led", 4096, NULL, 1, NULL);
    #endif
}

#endif

