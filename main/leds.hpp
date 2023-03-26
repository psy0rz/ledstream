#ifndef HEADER_LEDS
#define HEADER_LEDS

#include <FastLED.h>
#include "utils.h"

#define COLOR_ORDER GRB

CRGB leds[CONFIG_LEDSTREAM_CHANNELS][CONFIG_LEDSTREAM_LEDS_PER_CHANNEL];

void leds_init() {

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


// notify user via led 0 of channel 0 (async, return immeadatly)
void async_notify(CRGB rgb, int on, int total) {
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

// notify user via led 0 of channel 0 (sync, returns after one blink cycle)
void notify(CRGB rgb, int on, int total) {

    leds[0][0] = rgb;
    FastLED.show();
    vTaskDelay(on / portTICK_PERIOD_MS);
    leds[0][0] = CRGB::Black;
    FastLED.show();
    vTaskDelay((total - on) / portTICK_PERIOD_MS);
}




void progress_bar(int percentage) {
    static int last_percentage = -1;

    if (last_percentage == percentage)
        return;

    last_percentage = percentage;

    ESP_LOGI("progress", "%d%%", percentage);

    const int NUM_LEDS = 8;
    int num_leds_lit = ((percentage * NUM_LEDS) / 100);
//        static int flash_state = 0;

    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < num_leds_lit) {
            leds[0][i] = CRGB::DarkGreen;
        } else {
            leds[0][i] = CRGB::DarkRed;
        }
    }
    FastLED.show();
}

#endif

