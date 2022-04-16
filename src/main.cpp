#include "config.h"

#include <Arduino.h>
#include <WiFi.h>

#define FASTLED_ESP32_I2S true
#include <FastLED.h>


#define NUM_LEDS 300
#define STRIPS 2
CRGB leds[STRIPS][NUM_LEDS];

void wificheck()
{
  while (WiFi.status() != WL_CONNECTED) {

    FastLED.clear();
    FastLED.show();
    delay(500);
    leds[0][0]=CRGB::Red;
    FastLED.show();
    delay(500);


    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);


  }
}


void
setup()
{
  Serial.begin(115200);
  Serial.println("Booted\n");
  FastLED.addLeds<NEOPIXEL, 12>(leds[0], NUM_LEDS);
  FastLED.addLeds<NEOPIXEL, 13>(leds[1], NUM_LEDS);
  // FastLED.clear();
  FastLED.setBrightness(255);
  wificheck();
}



void loop()
{
  for (int i = 1; i < NUM_LEDS; i++) {
    leds[0][i-1]=CRGB::Black;
    leds[0][i] = CRGB::Red;
    leds[1][i-1]=CRGB::Black;
    leds[1][i] = CRGB::Green;
    // int start=millis();
    // FastLED.show();
    // int end=millis();
    // Serial.printf("tijd %i\n" ,end-start);
    // int start=millis();
    FastLED.show();
    // int end=millis();
    // Serial.println(end-start);
    delayMicroseconds(1000000);
    
  }
}
