#include "esp_log.h"
static const char* TAG = "ledstream";


#include "config.h"
#include "ledstreamer.hpp"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>


// #define FASTLED_ESP32_I2S true
#include <FastLED.h>
//#include <multicastsync.hpp>
//#include <udpbuffer.hpp>
//#include <qois.hpp>



CRGB leds[CHANNELS][LEDS_PER_CHAN];

//UdpBuffer udpBuffer = UdpBuffer();
//MulticastSync multicastSync = MulticastSync();
//Qois qois = Qois();
Ledstreamer ledstreamer = Ledstreamer((CRGB*)leds, CHANNELS * LEDS_PER_CHAN);


CRGB &getLed(uint16_t ledNr) {
    return (leds[ledNr / LEDS_PER_CHAN][ledNr % LEDS_PER_CHAN]);
}


bool duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0) {
    if (!starttime)
        return ((ledstreamer.multicastSync.remoteMillis() % total) < on);
    else
        return (((ledstreamer.multicastSync.remoteMillis() - starttime) % total) < on);
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

void wificheck() {
    if (WiFi.status() == WL_CONNECTED)
        return;

//    Serial.printf("Attempting to connect to WPA SSID: %s\n", ssid);
    ESP_LOGI(TAG, "Connecting to wifi...");
    while (WiFi.status() != WL_CONNECTED) {
        yield();
        notify(CRGB::Red, 125, 250);
    }
    ESP_LOGI(TAG, "Wifi connected.");



//    Serial.printf("MDNS mdns name is %s\n", ArduinoOTA.getHostname().c_str());
//    Serial.println(WiFi.localIP());

//    multicastSync.begin();
}

void
setup() {
    Serial.begin(115200);
//    Serial.println("ledstream: Booted\n");
//
//    ESP_LOGI(TAG, "info");
//    ESP_LOGD(TAG, "debub");
//
//    ESP_LOGE(TAG, "error2");
//
//
//    Serial.printf("ledstream: CPU=%dMhz\n", getCpuFrequencyMhz());

#ifdef CHANNEL0_PIN
    FastLED.addLeds<NEOPIXEL, CHANNEL0_PIN>(leds[0], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL1_PIN
    FastLED.addLeds<NEOPIXEL, CHANNEL1_PIN>(leds[1], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL2_PIN
    FastLED.addLeds<NEOPIXEL, CHANNEL2_PIN>(leds[2], LEDS_PER_CHAN);
#endif
#ifdef CHANNEL3_PIN
    FastLED.addLeds<NEOPIXEL, CHANNEL3_PIN>(leds[3], LEDS_PER_CHAN);
#endif

    FastLED.clear();
    FastLED.show();
    FastLED.setBrightness(255);
    WiFi.begin(ssid, pass);

    wificheck();
    ArduinoOTA.begin();
    // ArduinoOTA.setPassword("dsf09845jxczvjcxf"); doesnt work?
    ledstreamer.begin();
}


void loop() {

//    udpBuffer.begin(65000);
//    udpBuffer.reset();
//    unsigned long showTime = 0;
//    uint32_t lastTime = 0;

//    udpPacketStruct *packet = nullptr;
//    bool ready = false;


//    while (1) {


    //backgrond processes:
    wificheck();
    ArduinoOTA.handle();
    ledstreamer.handle();

//        multicastSync.handle();
//        udpBuffer.handle();

//        if (ready) {
//            // its time to output the prepared leds buffer?
//            if (multicastSync.remoteMillis() >= showTime) {
//                FastLED.show();
//                // Serial.printf("avail=%d, showtime=%d \n",
//                // udpBuffer.available(),showTime);
//
//                ready = false;
//            }
//        } else {
//            //prepare next frame
//
//
//            //need new packet
//            if (packet == nullptr)
//            {
//                if (udpBuffer.available() > 0) {
//                    packet = udpBuffer.readNext();
//                }
//            } else {
//
//            }
//        }


//        if (udpBuffer.available() > 0) {
//            packet = udpBuffer.readNext();

//                Serial.printf("frame time %u\n", packet->frame.time);
//                showTime = packet->frame.time + LAG;
//                qois.decode(packet->frame.data, packet->plen-sizeof(packet->frame.time), &leds[0][0], LED_COUNT);
//                ready = true;

//            } else {
//                if (multicastSync.synced())
//                    notify(CRGB::Green, 1000, 2000);
//                else
//                    notify(CRGB::Yellow, 500, 1000);
//            }
//        }
//    }
}
