#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// #define FASTLED_ESP32_I2S true
#include <FastLED.h>
#include <udpbuffer.hpp>
#include <multicastsync.hpp>

CRGB leds[CHANNELS][NUM_LEDS];

UdpBuffer udpBuffer = UdpBuffer();
MulticastSync multicastSync=MulticastSync();

void
wificheck()
{
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {

    FastLED.clear();
    FastLED.show();
    delay(500);
    leds[0][0] = CRGB::Red;
    FastLED.show();
    delay(500);

    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
  }
  leds[0][0] = CRGB::Green;
  FastLED.show();
  Serial.println(WiFi.localIP());
}

void
setup()
{
  Serial.begin(115200);
  Serial.println("Booted\n");
  Serial.printf("CPU=%dMhz\n", getCpuFrequencyMhz());

  Serial.println(sizeof(packetStruct));
  FastLED.addLeds<NEOPIXEL, 12>(leds[0], NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, 13>(leds[1], NUM_LEDS);
  FastLED.clear();
  FastLED.setBrightness(255);
  wificheck();
}

void
loop()
{

  multicastSync.begin();

  udpBuffer.begin(65000);
  udpBuffer.reset();
  // unsigned long lastTime = multicastSync.remoteMicros();
  unsigned long showTime=0;
  uint32_t lastTime=0;

  packetStruct* packet = NULL;
  bool ready = false;


  // float pidDelay=16300;
  // float nextDelay=pidDelay;


  // nextTime=multicastSync.remoteMicros()+interval*(BUFFER_FRAMES/2);

  // byte waitFrames=0;

  while (1) {

    multicastSync.recv();

    // // recv packets and calc framerate
    udpBuffer.recvNext();


    // ready to show next?
    if (ready) {
      if (multicastSync.remoteMillis() >=showTime+(16*10)) {
        FastLED.show();
          // Serial.printf("avail=%d, showtime=%d \n", udpBuffer.available(),showTime);

        // nextTime=nextTime+(interval*waitFrames);
        // Serial.println(waitFrames);
        ready = false;

      }
    }
    // prepare next
    else {

      if (udpBuffer.available() > 0) {
        if (packet==NULL)
          packet = udpBuffer.readNext();


        //data complete?
        if (packet->time!=lastTime)
        {
          ready=true;


          // Serial.printf("%d diff\n", lastTime-showTime);
          showTime=lastTime;

          lastTime=packet->time;


        }
        else
        {
          //update led array
          memcpy(leds[packet->channel], packet->pixels, sizeof(packet->pixels));
          packet=NULL;
        }
      }
    }
  }
}
