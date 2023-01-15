
static const char *TAG = "ledstream";


#include "config.h"
#include "ledstreamer.hpp"
#include "wifi.hpp"


// #define FASTLED_ESP32_I2S true
#include <FastLED.h>

//#include <multicastsync.hpp>
//#include <udpbuffer.hpp>
//#include <qois.hpp>

#define CHANNELS 2
#define LEDS_PER_CHAN 300

CRGB leds[CHANNELS][LEDS_PER_CHAN];

//UdpBuffer udpBuffer = UdpBuffer();
//TimeSync timeSync = TimeSync();
//Qois qois = Qois();
Ledstreamer ledstreamer = Ledstreamer((CRGB *) leds, CHANNELS * LEDS_PER_CHAN);


CRGB &getLed(uint16_t ledNr) {
    return (leds[ledNr / LEDS_PER_CHAN][ledNr % LEDS_PER_CHAN]);
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


void
setup() {


#ifdef CHANNEL0_PIN
    FastLED.addLeds<WS2811, CHANNEL0_PIN>(leds[0], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL1_PIN
    FastLED.addLeds<WS2811, CHANNEL1_PIN>(leds[1], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL2_PIN
    FastLED.addLeds<WS2811, CHANNEL2_PIN>(leds[2], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL3_PIN
    FastLED.addLeds<WS2811, CHANNEL3_PIN>(leds[3], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL4_PIN
    FastLED.addLeds<WS2811, CHANNEL4_PIN>(leds[4], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL5_PIN
    FastLED.addLeds<WS2811, CHANNEL5_PIN>(leds[5], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL6_PIN
    FastLED.addLeds<WS2811, CHANNEL6_PIN>(leds[6], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL7_PIN
    FastLED.addLeds<WS2811, CHANNEL7_PIN>(leds[7], LEDS_PER_CHAN);
#endif

    FastLED.clear();
    FastLED.show();
    FastLED.setBrightness(255);
    wifi_init_sta();

    wificheck();
    ledstreamer.begin();
}


void loop() {

//    udpBuffer.begin(65000);
//    udpBuffer.reset();
//    unsigned long showTime = 0;
//    uint32_t lastTime = 0;

//    udpPacketStruct *currentPacket = nullptr;
//    bool ready = false;


//    while (1) {


    //backgrond processes:
    // wificheck();
    ledstreamer.handle();


    if (ledstreamer.idle()) {
        if (ledstreamer.timeSync.synced())
            notify(CRGB::Green, 1000, 2000);
        else
            notify(CRGB::Yellow, 500, 1000);
    }

}
