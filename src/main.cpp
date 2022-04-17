#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// #define FASTLED_ESP32_I2S true
#include <FastLED.h>

#define NUM_LEDS 300
#define CHANNELS 2
CRGB leds[CHANNELS][NUM_LEDS];

struct packetStruct
{
  byte frame;
  byte channel;
  CRGB pixels[NUM_LEDS];
} ;

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
  WiFiUDP udp;
  udp.begin(65000);

  byte lastFrame=0;
  packetStruct packet;
  word plen;

  unsigned long avgDelay=0;
  unsigned long lastFrameTime=0;


  while (1) {
    plen = udp.parsePacket();

    if (plen)
    {
      if (plen != sizeof(packet)) {
        Serial.printf("Ignored packet with length %d\n", plen);
        udp.flush();
      }
      else
      {
        udp.read((char*)&packet, sizeof(packet));
        if (packet.channel>=CHANNELS)
        {
          Serial.printf("Illegal channel number %d\n", packet.channel);
        }
        else
        {
          // Serial.printf("Frame %d, channel %d\n", packet.frame, packet.channel);
          if (packet.frame!=lastFrame)
          {

            //calc delay
            unsigned long now=micros();
            avgDelay=(avgDelay*0.99)+(now-lastFrameTime)*0.01;
            // Serial.printf("avg=%d\n", avgDelay);
            lastFrameTime=now;

            FastLED.show();
            lastFrame=packet.frame;
            byte diff=packet.frame-lastFrame;
            if (diff>1)
            {
              Serial.printf("Missed %d frames\n", diff);
            }

          }
          memcpy(leds[packet.channel], packet.pixels, sizeof(packet.pixels));
        }
      }
    }
  }
}
