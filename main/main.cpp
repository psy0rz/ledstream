#include "ledstreamer.hpp"
#include "wifi.hpp"
#include <FastLED.h>
#include "ethernet.h"
#include "ota.hpp"

#include "leds.hpp"

//#include "fileserver.hpp"

static const char *MAIN_TAG = "main";


OTAUpdater ota_updater = OTAUpdater(reinterpret_cast<CRGB *>(leds));
Ledstreamer ledstreamer = Ledstreamer();


//CRGB &getLed(uint16_t ledNr) {
//    return (leds[ledNr / CONFIG_LEDSTREAM_LEDS_PER_CHANNEL][ledNr % CONFIG_LEDSTREAM_LEDS_PER_CHANNEL]);
//}





extern "C" [[noreturn]] __attribute__((unused)) void app_main(void) {

    //settle
    delay(250);


    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
    ethernet_init();

//    wificheck();
    ledstreamer.begin(65000);


//    auto lastTime = millis();
    ESP_LOGI(MAIN_TAG, "RAM left %d", esp_get_free_heap_size());

    //main task
    while (true) {
        ledstreamer.process();

//        if (millis() - lastTime > 1000) {
//            ESP_LOGI(MAIN_TAG, "heartbeat");
//
//            lastTime = millis();
//
//        }





        ///TEST
//        FastLED.clear();
//        test=(test+1)%300;
//        leds[0][test]=CRGB::Red;
//        leds[1][test]=CRGB::Green;
//        FastLED.show();
//        vTaskDelay(16/portTICK_PERIOD_MS);




//        if (ledstreamer.idle()) {
//            if (ledstreamer.timeSync.synced())
//                notify(CRGB::Green, 1000, 2000);
//            else
//                notify(CRGB::Yellow, 500, 1000);
//        }
    }

}
