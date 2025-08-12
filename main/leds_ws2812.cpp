
#include "esp_log.h"
#include "sdkconfig.h"
#ifdef CONFIG_LEDSTREAM_MODE_WS2812
#include "leds.hpp"
#include <FastLED.h>


#define COLOR_ORDER GRB

CRGB leds[CONFIG_LEDSTREAM_CHANNELS][CONFIG_LEDSTREAM_LEDS_PER_CHANNEL];

//stuff to determine which pixel to set
uint16_t leds_pixels_per_channel;
uint16_t leds_channel_nr;
uint16_t leds_pixel_nr;

void leds_init()
{
    ESP_LOGI("leds", "Initializing FastLED-idf library...");


#if CONFIG_LEDSTREAM_CHANNELS > 0
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL0_PIN, COLOR_ORDER>(leds[0], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 1
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL1_PIN, COLOR_ORDER>(leds[1], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 2
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL2_PIN, COLOR_ORDER>(leds[2], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 3
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL3_PIN, COLOR_ORDER>(leds[3], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 4
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL4_PIN, COLOR_ORDER>(leds[4], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 5
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL5_PIN, COLOR_ORDER>(leds[5], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 6
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL6_PIN, COLOR_ORDER>(leds[6], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif
#if CONFIG_LEDSTREAM_CHANNELS > 7
    FastLED.addLeds<WS2811, CONFIG_LEDSTREAM_CHANNEL7_PIN, COLOR_ORDER>(leds[7], CONFIG_LEDSTREAM_LEDS_PER_CHANNEL);
#endif

    FastLED.clear();
    FastLED.show();

    if (CONFIG_LEDSTREAM_MAX_MILLIAMPS > 0)
        FastLED.setMaxPowerInVoltsAndMilliamps(5, CONFIG_LEDSTREAM_MAX_MILLIAMPS);
}

void IRAM_ATTR leds_reset()
{
    leds_channel_nr=0;
    leds_pixel_nr=0;

    if (leds_pixels_per_channel == 0)
        leds_pixels_per_channel = CONFIG_LEDSTREAM_LEDS_PER_CHANNEL;

}

void IRAM_ATTR leds_show()
{
    FastLED.show();
}


void IRAM_ATTR leds_setNextPixel(const uint8_t r, const uint8_t g, const uint8_t b)
{
    //only set if we're in range
    if (leds_channel_nr < CONFIG_LEDSTREAM_CHANNELS && leds_pixel_nr < CONFIG_LEDSTREAM_LEDS_PER_CHANNEL)
    {
        leds[leds_channel_nr][leds_pixel_nr].r = r;
        leds[leds_channel_nr][leds_pixel_nr].g = g;
        leds[leds_channel_nr][leds_pixel_nr].b = b;
    }
    leds_pixel_nr++;
    if (leds_pixel_nr == leds_pixels_per_channel)
    {
        leds_channel_nr++;
        leds_pixel_nr = 0;
    }
}


// notify user via led 0 of channel 0 (async, return immeadatly)
//void async_notify(CRGB rgb, int on, int total) {
//    static bool lastState = false;
//
//    if (duty_cycle(on, total) != lastState) {
//        lastState = !lastState;
//
//        if (lastState)
//            leds[0][0] = rgb;
//        else
//            leds[0][0] = CRGB::Black;
//        FastLED.show();
//    }
//}

//sets status led
//void status_led(CRGB rgb)
//{
//    leds[0][0] = rgb;
//    FastLED.show();
//
//}
//
//
//// notify user via led 0 of channel 0 (sync, returns after one blink cycle)
//void blink_led(CRGB rgb, int on, int total) {
//    status_led(rgb);
//    vTaskDelay(on / portTICK_PERIOD_MS);
//    status_led(CRGB::Black);
//    vTaskDelay((total - on) / portTICK_PERIOD_MS);
//}
//


#endif
