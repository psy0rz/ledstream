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
  unsigned long nextTime;
  uint32_t lastTime=0;

  packetStruct* packet = NULL;
  bool ready = false;

  unsigned long interval=16000;

  // float pidDelay=16300;
  // float nextDelay=pidDelay;


  // nextTime=multicastSync.remoteMicros()+interval*(BUFFER_FRAMES/2);
  nextTime=0;

  byte waitFrames=0;

  while (1) {

    multicastSync.recv();
    continue;

    // recv packets and calc framerate
    udpBuffer.recvNext();

    //overrun
    if (udpBuffer.available()==BUFFER-1)
    {
      Serial.println("Buffer overrun\n");
      while(udpBuffer.available()>BUFFER/2)
      {
        udpBuffer.recvNext();
        udpBuffer.readNext();
      }
      ready=false;
      nextTime=multicastSync.remoteMillis();
    }

    //underrun
    if (udpBuffer.available()==0)
    {
      Serial.println("Buffer underrun\n");
      while(udpBuffer.available()<BUFFER/2)
      {
        udpBuffer.recvNext();
      }
      ready=false;
      nextTime=multicastSync.remoteMillis();
    }


    // ready to show next?
    if (ready) {
      if (multicastSync.remoteMillis() >=nextTime) {
        FastLED.show();
        nextTime=nextTime+(interval*waitFrames);
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
          // if (waitFrames==0) //startup
          //   waitFrames=1;
          // else
          // {
          //   waitFrames=packet->frame-lastFrame;
          //   if (waitFrames>5)
          //     waitFrames=1;
          // }
          waitFrames=1;
          // Serial.printf("%d - %d = %d \n ", packet->frame, lastFrame, waitFrames);
          lastTime=packet->time;

          //pid 
          // int  delta=(int)udpBuffer.lastFrame-(int)lastFrame;
          // if (delta<-128)
          //   delta=delta+256;

          // float error=(BUFFER_FRAMES/2)-delta;

          // pidDelay=pidDelay+(error*0.1);
          // nextDelay=pidDelay+(error*100);

          // if (lastFrame%60==0)
            Serial.printf("avail=%d, \n", udpBuffer.available());
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
