
#include "wifi.hpp"
#include "ethernet.h"
#include "ota.hpp"

#include "leds.hpp"

#include "qois.hpp"

#include "ledstreamer_http.hpp"
#include "ledstreamer_flash.hpp"

static const char* MAIN_TAG = "main";


OTAUpdater ota_updater = OTAUpdater();

#define MONITOR_TASK_PERIOD_MS 1000

static TaskStatus_t* prevTaskArray = NULL;
static uint32_t prevTotalRunTime = 0;

void timing_test(void* p)
{
    uint32_t c = 0;

    int on = 0;


    while (1)
    {
        on = (on + 1) % 64;

        for (int y = 0; y < 32; y++)
        {
            for (int x = 0; x < 64; x++)
            {
                if (on == x)
                    leds_setNextPixel(255, 255, 255);
                else
                    leds_setNextPixel(0, 0, 0);
            }
        }


        c = c + 1000000 / 60; // c = c + 1000000/1;
        timing_wait_until_us(c);
        leds_show();
        leds_reset();
    }
}


extern "C" __attribute__((unused)) void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Starting ledstreamer...");

    //Initialize NVS
    ESP_LOGI(MAIN_TAG, "Prepare NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(MAIN_TAG, "Erasing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

//     // Initialize TCP/IP network interface (should be called only once in application)
//     ESP_ERROR_CHECK(esp_netif_init());
//     // Create default event loop that running in background
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//
//     fileserver_init();
//     leds_init();

        wifi_init_sta();
// #if CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET
//
//      ethernet_init();
// #endif
//
//     //main task
//     ESP_LOGI(MAIN_TAG, "Start mainloop:");
//
//     // xTaskCreate(ledstreamer_udp_task, "ledstreamer_udp_task", 4096, nullptr, 1, nullptr);
//
//     timing_init();
//
//     ledstreamer_http_init();
//     ledstreamer_flash_init();


}
