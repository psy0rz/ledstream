#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// #define FASTLED_ESP32_I2S true
#include <FastLED.h>

#define NUM_LEDS 300
#define STRIPS 2
CRGB leds[STRIPS][NUM_LEDS];




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
  Serial.println(WiFi.localIP());
}

void
setup()
{
  Serial.begin(115200);
  Serial.println("Booted\n");
  FastLED.addLeds<NEOPIXEL, 12>(leds[0], NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, 13>(leds[1], NUM_LEDS);
  FastLED.clear();
  FastLED.setBrightness(255);
  wificheck();
}


 char pbuf[1500];

void
loop()
{
  WiFiUDP udp;

  udp.begin(65000);
  int lasttime;

  while(1)
  {
    int plen=udp.parsePacket();
    Serial.println(ESP.getFreeHeap());
    // Serial.println("kk");
    if (plen!=0)
    {
      Serial.printf("delta %d, recv %d\n", plen, millis()-lasttime);
      lasttime=millis();
      udp.read(pbuf, sizeof(pbuf));
    }
    else
    {
      delay(1000);
    }

  }


}
