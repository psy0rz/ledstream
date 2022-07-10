#include "config.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// #define FASTLED_ESP32_I2S true
#include "utils.h"
#include <FastLED.h>
#include <multicastsync.hpp>
#include <udpbuffer.hpp>

CRGB leds[CHANNELS][LEDS_PER_CHAN];

UdpBuffer udpBuffer = UdpBuffer();
MulticastSync multicastSync = MulticastSync();



CRGB &getLed(uint16_t ledNr) {
    return (leds[ledNr / LEDS_PER_CHAN][ledNr % LEDS_PER_CHAN]);
}

//TODO: bounds checking
void parsePacket(packetStruct *packet) {
    uint16_t ledNr = 0;
    uint16_t cmdNr = 0;


    while (cmdNr < DATA_LEN && ledNr < LED_COUNT) {
        uint8_t cmdType = packet->data[cmdNr] & TYPE_MASK;
        uint8_t cmdValue = packet->data[cmdNr] & VALUE_MASK;
        switch (cmdType) {
            case SKIP_TYPE: {
                ledNr = ledNr + cmdValue;
                cmdNr++;
                break;
            }
            case REPEAT_TYPE: {
                CRGB sourceRgb = getLed(ledNr - 1);
                while (cmdValue > 0) {
                    CRGB targetRgb = getLed(ledNr);
                    memcpy(targetRgb.raw, sourceRgb.raw, 3);
                    ledNr++;
                    cmdValue--;
                }
                cmdNr++;
                break;
            }
            case COLOR_REF_TYPE: {
                CRGB sourceRgb = getLed(ledNr - cmdValue);
                CRGB targetRgb = getLed(ledNr);
                memcpy(targetRgb.raw, sourceRgb.raw, 3);
                cmdNr++;
                ledNr++;
                break;
            }
            case COLOR_FULL_TYPE: {
                Serial.printf("full led=%d, cmd=%d r=%d, g=%d, b=%d\n", ledNr, cmdNr,
                              packet->data[cmdNr + 1],
                              packet->data[cmdNr + 2],
                              packet->data[cmdNr + 3]
                              );
                CRGB targetRgb = getLed(ledNr);
                memcpy(targetRgb.raw, &packet->data[cmdNr + 1], 3);
                ledNr++;
                cmdNr = cmdNr + 4; // skip 3 color bytes as well
                break;
            }
            default:
                Serial.printf("%s: wtf\n", __FUNCTION__);
                break;
        }
    }
}


// dutycycle function to make stuff blink without using memory
// returns true during 'on' ms, with total periode time 'total' ms.
bool
duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0) {
    if (!starttime)
        return ((multicastSync.remoteMillis() % total) < on);
    else
        return (((multicastSync.remoteMillis() - starttime) % total) < on);
}

// notify user via led and clearing display
// also note that blinking should be in sync between different display via
// multicastsync
void
notify(CRGB rgb, int on, int total) {
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
    Serial.begin(115200);
    Serial.println("ledstream: Booted\n");
    Serial.printf("ledstream: CPU=%dMhz\n", getCpuFrequencyMhz());

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

    ArduinoOTA.begin();
    // ArduinoOTA.setPassword("dsf09845jxczvjcxf"); doesnt work?
}

void
wificheck() {
    if (WiFi.status() == WL_CONNECTED)
        return;

    Serial.printf("Attempting to connect to WPA SSID: %s\n", ssid);
    while (WiFi.status() != WL_CONNECTED) {
        notify(CRGB::Red, 125, 250);
    }
    Serial.printf("MDNS mdns name is %s\n", ArduinoOTA.getHostname().c_str());
    Serial.println(WiFi.localIP());

    multicastSync.begin();
}


void
loop() {

    udpBuffer.begin(65000);
    udpBuffer.reset();
    unsigned long showTime = 0;
    uint32_t lastTime = 0;

    packetStruct *packet = NULL;
    bool ready = false;

    while (1) {


        //backgrond processes:
        wificheck();
        ArduinoOTA.handle();
        multicastSync.recv();
        udpBuffer.recvNext();

        if (ready) {
            // its time to output the prepared leds buffer?
            if (multicastSync.remoteMillis() >= showTime) {
                FastLED.show();
                // Serial.printf("avail=%d, showtime=%d \n",
                // udpBuffer.available(),showTime);

                ready = false;
            }
        } else {
            // parse next packet and prepare leds buffer

            if (udpBuffer.available() > 0) {
                packet = udpBuffer.readNext();

                Serial.printf("packet time %u\n", packet->time);
                showTime = packet->time + LAG;
                parsePacket(packet);
                ready = true;

            } else {
                if (multicastSync.synced())
                    notify(CRGB::Green, 1000, 2000);
                else
                    notify(CRGB::Yellow, 500, 1000);
            }
        }
    }
}
