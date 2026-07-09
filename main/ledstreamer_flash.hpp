//plays animation from flash in a loop

#ifndef LEDSTREAMER_FLASH_HPP
#define LEDSTREAMER_FLASH_HPP


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char* LEDSTREAMER_FLASH_TAG = "flash";

bool ledstreamer_flash_run = false;
bool ledstreamer_flash_running = false;



//start looping from flash
inline void ledstreamer_flash_start()
{
    if (!ledstreamer_flash_running)
    {
        ESP_LOGI(LEDSTREAMER_FLASH_TAG, "starting replay");
        ledstreamer_flash_run = true;
    }
}

// stop looping from flash
inline void ledstreamer_flash_stop()
{
    //always clear the run flag: while stopped the task polls it only once a
    //second, so a recent ledstreamer_flash_start() may not have set 'running'
    //yet -- leaving 'run' set would start a replay concurrently with the live
    //stream (both racing on the shared frame timer)
    ledstreamer_flash_run = false;
    if (ledstreamer_flash_running)
        ESP_LOGI(LEDSTREAMER_FLASH_TAG, "stopping replay");
    while (ledstreamer_flash_running)
        vTaskDelay(10 / portTICK_PERIOD_MS);
}

// Task function
inline void ledstreamer_flash_task(void* arg)
{
    fileserver_ctx* ctx = nullptr;

    while (true)
    {
        //stopped
        ledstreamer_flash_running = false;
        while (!ledstreamer_flash_run)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ledstreamer_flash_running = true;

        // ESP_LOGI(LEDSTREAMER_FLASH_TAG, "running from flash");
        ctx = fileserver_open(false);
        if (ctx == nullptr)
        {
            //no recording on flash: same as stopped mode, do nothing with a blank screen
            leds_clear();
            leds_show();
            ledstreamer_flash_run=false;
        }
        else
        {
            ESP_LOGI(LEDSTREAMER_FLASH_TAG, "replaying from flash");
            //running
            //no playout buffer: the data comes from local flash, not the network
            timing_reset(false);
            //recorded streams start at the beginning of a connection, so every
            //replay loop must restart the decoder from the same fresh state
            qois_resetStream();
            while (ledstreamer_flash_run && fileserver_read(ctx))
            {
                qois_decodeBytes(static_cast<const uint8_t*>(ctx->buffer), ctx->buffered, 0);
            }
            const bool read_error = ctx->read_error;
            fileserver_close(ctx);
            if (read_error)
            {
                //unreadable recording: same as stopped mode, do nothing with a blank screen
                leds_clear();
                leds_show();
                ledstreamer_flash_run=false;
            }
        }
    }
}


inline void ledstreamer_flash_init()
{
    xTaskCreatePinnedToCore(ledstreamer_flash_task, "ledstreamer_flash_task", 4096, nullptr, 10, nullptr, 0);
    ledstreamer_flash_run = true;
}


#endif //LEDSTREAMER_FLASH_HPP
