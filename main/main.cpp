#include "wifi.hpp"
#include "ethernet.h"
#include "ota.hpp"

#include "leds.hpp"

#include "qois.hpp"
#include "ledstreamer_udp.hpp"
#include "ledstreamer_http.hpp"
// #include "fileserver.hpp"

static const char* MAIN_TAG = "main";


OTAUpdater ota_updater = OTAUpdater();


//CRGB &getLed(uint16_t ledNr) {
//    return (leds[ledNr / CONFIG_LEDSTREAM_LEDS_PER_CHANNEL][ledNr % CONFIG_LEDSTREAM_LEDS_PER_CHANNEL]);
//}


LedstreamerUDP ledstreamer_udp = LedstreamerUDP();


[[noreturn]] void ledstreamer_udp_task(void* args)

{
    while (true)
        ledstreamer_udp.process();
}


extern "C" __attribute__((unused)) void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Starting ledstreamer...");


    //settle
    vTaskDelay(250 / portTICK_PERIOD_MS);


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

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    leds_init();

    wifi_init_sta();

#if CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET

     ethernet_init();
#endif
    // fileserver_start();

    //    wificheck();
    ledstreamer_udp.begin(65000);
    //    auto lastTime = millis();
    //    ESP_LOGI(MAIN_TAG, "RAM left %lu", esp_get_free_heap_size());

    //main task
    ESP_LOGI(MAIN_TAG, "Start mainloop:");

    xTaskCreate(ledstreamer_udp_task, "ledstreamer_udp_task", 4096, nullptr, 1, nullptr);

    ledstreamer_http_init();
}
