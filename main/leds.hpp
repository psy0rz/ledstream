#ifndef HEADER_LEDS
#define HEADER_LEDS


#include "utils.h"

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

MatrixPanel_I2S_DMA *dma_display = nullptr;

#ifdef CONFIG_LEDSTREAM_MODE_WS2812
#include <FastLED.h>

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
void status_led(CRGB rgb)
{
    leds[0][0] = rgb;
    FastLED.show();

}


// notify user via led 0 of channel 0 (sync, returns after one blink cycle)
void blink_led(CRGB rgb, int on, int total) {
    status_led(rgb);
    vTaskDelay(on / portTICK_PERIOD_MS);
    status_led(CRGB::Black);
    vTaskDelay((total - on) / portTICK_PERIOD_MS);
}





#endif

#ifdef CONFIG_LEDSTREAM_MODE_HUB75

void leds_init()
{

    HUB75_I2S_CFG mxconfig(/* width = */ 64, /* height = */ 32, /* chain = */ 1);

        // HUB75_I2S_CFG mxconfig(/* width = */ 64, /* height = */ 64, /* chain = */ 1);

        dma_display = new MatrixPanel_I2S_DMA(mxconfig);
        dma_display->begin();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
        dma_display->setBrightness8(255);
        dma_display->clearScreen();
    dma_display->drawPixelRGB888(10,10,255,255,255);
    dma_display->drawPixelRGB888(11,10,255,255,255);
    dma_display->drawPixelRGB888(10,10,255,255,255);
    dma_display->drawPixelRGB888(10,10,255,255,255);


}

#endif



void progress_bar(int percentage) {
    static int last_percentage = -1;

    if (last_percentage == percentage)
        return;

    last_percentage = percentage;

    ESP_LOGI("progress", "%d%%", percentage);

#ifdef CONFIG_LEDSTREAM_MODE_WS2812
    const int bar_length = 8;
    int num_leds_lit = ((percentage * bar_length) / 100);
//        static int flash_state = 0;

    for (int i = 0; i < bar_length; i++) {
        if (i < num_leds_lit) {
            leds[0][i] = CRGB::DarkGreen;
        } else {
            leds[0][i] = CRGB::DarkRed;
        }
    }
    FastLED.show();
#endif
}



#endif

