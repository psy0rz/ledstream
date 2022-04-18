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
  unsigned long lastTime = micros();
  unsigned long now;
  byte lastFrame = 0;

  packetStruct* packet = NULL;
  bool ready = false;

  float pidDelay=16300;
  float nextDelay=pidDelay;

  while (1) {

    multicastSync.recv();

    // recv packets and calc framerate
    udpBuffer.recvNext();

    // ready to show next?
    if (ready) {
      now = micros();
      if (now - lastTime >= nextDelay) {
        FastLED.show();
        lastTime = now;
        ready = false;

      }
    }
    // prepare next
    else {

      if (udpBuffer.available() > 0) {
        if (packet==NULL)
          packet = udpBuffer.readNext();


        if (packet->frame!=lastFrame)
        {
          ready=true;
          lastFrame=packet->frame;

          //pid 
          // float error=(BUFFER/2)-udpBuffer.available(); //try to keep buffer at 50%
          int  delta=(int)udpBuffer.lastFrame-(int)lastFrame;
          if (delta<-128)
            delta=delta+256;

          float error=(BUFFER_FRAMES/2)-delta;

          pidDelay=pidDelay+(error*0.1);
          nextDelay=pidDelay+(error*100);

          // unsigned long d=(micros()-udpBuffer.lastFrameTime) delta

          // if (lastFrame%60==0)
          //   Serial.printf("avail=%d, error=%f, pidDelay=%f, nextDelay=%f\n", udpBuffer.available(), error, pidDelay, nextDelay);
          // Serial.printf("lastrecvv=%d lastshow=%d delta=%d\n", udpBuffer.lastFrame, lastFrame, delta);

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
