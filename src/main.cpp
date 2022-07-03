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

// dutycycle function to make stuff blink without using memory
// returns true during 'on' ms, with total periode time 'total' ms.
bool
duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0)
{
  if (!starttime)
    return ((multicastSync.remoteMillis() % total) < on);
  else
    return (((multicastSync.remoteMillis() - starttime) % total) < on);
}

// notify user via led and clearing display
// also note that blinking should be in sync between different display via
// multicastsync
void
notify(CRGB rgb, int on, int total)
{
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
setup()
{
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
wificheck()
{
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
loop()
{

  udpBuffer.begin(65000);
  udpBuffer.reset();
  unsigned long showTime = 0;
  uint32_t lastTime = 0;

  packetStruct* packet = NULL;
  bool ready = false;

  while (1) {

    wificheck();
  ArduinoOTA.handle();

    multicastSync.recv();

    // recv packets and calc framerate
    udpBuffer.recvNext();

    // ready to show next?
    if (ready) {
      if (multicastSync.remoteMillis() >= showTime) {
        FastLED.show();
        // Serial.printf("avail=%d, showtime=%d \n",
        // udpBuffer.available(),showTime);

        ready = false;
      }
    }
    // prepare next
    else {

      if (udpBuffer.available() > 0) {
        if (packet == NULL)
          packet = udpBuffer.readNext();

        // data complete?
        if (packet->time != lastTime) {
          ready = true;

          // Serial.printf("%d diff\n", lastTime-showTime);
          showTime = lastTime + LAG;

          lastTime = packet->time;

        } else {
          // update led array
          memcpy(leds[packet->channel], packet->pixels, sizeof(packet->pixels));
          packet = NULL;
        }
      } else {
        if (multicastSync.synced())
          notify(CRGB::Green, 1000, 2000);
        else
          notify(CRGB::Yellow, 500, 1000);
      }
    }
  }
}
