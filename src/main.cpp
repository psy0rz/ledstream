#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// #define FASTLED_ESP32_I2S true
#include <FastLED.h>
#include <udpbuffer.hpp>
#include <multicastsync.hpp>
#include "utils.h"

CRGB leds[CHANNELS][NUM_LEDS];

UdpBuffer udpBuffer = UdpBuffer();
MulticastSync multicastSync=MulticastSync();


//dutycycle function to make stuff blink without using memory 
//returns true during 'on' ms, with total periode time 'total' ms.
bool duty_cycle(unsigned long on, unsigned long total, unsigned long starttime=0)
{
  if (!starttime)
    return((multicastSync.remoteMillis()%total)<on);
  else
    return(((multicastSync.remoteMillis()-starttime)%total)<on);
}

//notify user via led and clearing display 
//also note that blinking should be in sync between different display via multicastsync
void notify(CRGB rgb, int on, int total)
{
    static bool lastState=false;

    if (duty_cycle(on, total)!=lastState)
    {
      lastState=!lastState;

      FastLED.clear();
      if (lastState)
        leds[0][0]=rgb;
      FastLED.show();
    }
}


void
wificheck()
{
  WiFi.begin(ssid, pass);
  Serial.printf("Attempting to connect to WPA SSID: %s\n", ssid);
  while (WiFi.status() != WL_CONNECTED) {
    notify(CRGB::Red,125,250);

  }
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
  FastLED.show();
  FastLED.setBrightness(255);
  wificheck();
}

void
loop()
{

  multicastSync.begin();

  udpBuffer.begin(65000);
  udpBuffer.reset();
  unsigned long showTime=0;
  uint32_t lastTime=0;

  packetStruct* packet = NULL;
  bool ready = false;



  while (1) {

    multicastSync.recv();

    // recv packets and calc framerate
    udpBuffer.recvNext();


    // ready to show next?
    if (ready) {
      if (multicastSync.remoteMillis() >=showTime) {
        FastLED.show();
          // Serial.printf("avail=%d, showtime=%d \n", udpBuffer.available(),showTime);

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
          showTime=lastTime+LAG;

          lastTime=packet->time;


        }
        else
        {
          //update led array
          memcpy(leds[packet->channel], packet->pixels, sizeof(packet->pixels));
          packet=NULL;
        }
      }
      else
      {
        notify(CRGB::Green,1000,2000);

      }
    }
  }
}
