#include "ledstreamer.hpp"
#include "wifi.hpp"
#include <FastLED.h>
#include "ethernet.h"


#define COLOR_ORDER GRB

//note: is there a better way of doing this?
#ifdef CONFIG_LEDSTREAM_CHANNEL7_PIN
#define CHANNELS 8
#elif CONFIG_LEDSTREAM_CHANNEL6_PIN
#define CHANNELS 7
#elif CONFIG_LEDSTREAM_CHANNEL5_PIN
#define CHANNELS 6
#elif CONFIG_LEDSTREAM_CHANNEL4_PIN
#define CHANNELS 5
#elif CONFIG_LEDSTREAM_CHANNEL3_PIN
#define CHANNELS 4
#elif CONFIG_LEDSTREAM_CHANNEL2_PIN
#define CHANNELS 3
#elif CONFIG_LEDSTREAM_CHANNEL1_PIN
#define CHANNELS 2
#elif CONFIG_LEDSTREAM_CHANNEL0_PIN
#define CHANNELS 1
#endif


CRGB leds[CHANNELS][CONFIG_LEDSTREAM_LEDS_PER_CHANNEL];

static const char *MAIN_TAG = "main";


Ledstreamer ledstreamer = Ledstreamer((CRGB *) leds, CHANNELS * CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);


CRGB &getLed(uint16_t ledNr) {
    return (leds[ledNr / CONFIG_LEDSTREAM_LEDS_PER_CHANNEL][ledNr % CONFIG_LEDSTREAM_LEDS_PER_CHANNEL]);
}


bool duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0) {
    if (!starttime)
        return ((ledstreamer.timeSync.remoteMillis() % total) < on);
    else
        return (((ledstreamer.timeSync.remoteMillis() - starttime) % total) < on);
}

// notify user via led and clearing display
// also note that blinking should be in sync between different display via
// multicastsync
void notify(CRGB rgb, int on, int total) {
    static bool lastState = false;

    if (duty_cycle(on, total) != lastState) {
        lastState = !lastState;

        if (lastState)
            leds[0][0] = rgb;
        else
            leds[0][0] = CRGB::Black;
        FastLED.show();
    }
}


extern "C" void app_main(void) {

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

    wifi_init_sta();
    ethernet_init();

#ifdef CONFIG_LEDSTREAM_CHANNEL0_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL0_PIN, COLOR_ORDER>(leds[0], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL1_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL1_PIN, COLOR_ORDER>(leds[1], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL2_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL2_PIN, COLOR_ORDER>(leds[2], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL3_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL3_PIN, COLOR_ORDER>(leds[3], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL4_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL4_PIN, COLOR_ORDER>(leds[4], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL5_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL5_PIN, COLOR_ORDER>(leds[5], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL6_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL6_PIN, COLOR_ORDER>(leds[6], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#ifdef CONFIG_LEDSTREAM_CHANNEL7_PIN
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL7_PIN, COLOR_ORDER>(leds[7], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif

    FastLED.clear();
    FastLED.show();

//    FastLED.setBrightness(50);
    if (CONFIG_LEDSTREAM_MAX_MILLIAMPS > 0)
        FastLED.setMaxPowerInVoltsAndMilliamps(5, CONFIG_LEDSTREAM_MAX_MILLIAMPS);
//    wificheck();
    ledstreamer.begin(65000);


    auto lastTime = millis();
    ESP_LOGI(MAIN_TAG, "RAM left %d", esp_get_free_heap_size());


    while (1) {
        ledstreamer.process();

        if (millis() - lastTime > 1000) {
            ESP_LOGI(MAIN_TAG, "heartbeat");

            lastTime = millis();

        }


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
