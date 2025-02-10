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

        ESP_LOGI(LEDSTREAMER_FLASH_TAG, "running from flash");
        ctx = fileserver_open(false);
        if (ctx == nullptr)
        {
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
        else
        {
            //running
            qois_reset();
            while (ledstreamer_flash_run && fileserver_read(ctx))
            {
                qois_decodeBytes(static_cast<const uint8_t*>(ctx->buffer), ctx->buffered, 0);
            }
            fileserver_close(ctx);
        }
    }
}

//start looping from flash
inline void ledstreamer_flash_start()
{
    if (!ledstreamer_flash_running)
    {
        ESP_LOGI(LEDSTREAMER_FLASH_TAG, "start playing from flash memory");
        while (!ledstreamer_flash_running)
        {
            ledstreamer_flash_run = true;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

// stop looping from flash
inline void ledstreamer_flash_stop()
{
    if (ledstreamer_flash_running)
    {
        ESP_LOGI(LEDSTREAMER_FLASH_TAG, "stopping");
        while (ledstreamer_flash_running)
        {
            ledstreamer_flash_run = false;
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

inline void ledstreamer_flash_init()
{
    xTaskCreate(ledstreamer_flash_task, "ledstreamer_flash_task", 4096, nullptr, 10, nullptr);
    ledstreamer_flash_run = true;
}


#endif //LEDSTREAMER_FLASH_HPP
