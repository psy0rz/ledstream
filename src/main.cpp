#include "config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// #define FASTLED_ESP32_I2S true
#include <FastLED.h>

#define NUM_LEDS 300
#define CHANNELS 2
// nr of packets to buffer. higher is smoother but more lag.
#define BUFFER 10 * CHANNELS

CRGB leds[CHANNELS][NUM_LEDS];

struct packetStruct
{
  byte frame;
  byte channel;
  CRGB pixels[NUM_LEDS];
};

// circular udp packet buffer
class UdpBuffer
{
public:
  packetStruct packets[BUFFER];
  byte readIndex;
  byte recvIndex;

  byte lastFrame;
  unsigned long avgDelay;
  unsigned long lastFrameTime;

  WiFiUDP udp;

  UdpBuffer()
   {  }

  void begin(int port)
  {
    udp.begin(port);
  }

  void reset()
  {
    readIndex = 0;
    recvIndex = 0;
    lastFrame = 0;
    avgDelay = 16666; // start with 60fps
    lastFrameTime = micros();
  }

  // receive and store next udp packet if its available.
  // returns true if new packet
  bool recvNext()
  {
    int plen = udp.parsePacket();
    if (plen) {
      if (plen != sizeof(packetStruct)) {
        Serial.printf("Ignored packet with length %d\n", plen);
        udp.flush();
      } else {
        const packetStruct& packet = packets[recvIndex];

        // read and store packet
        udp.read((char*)&packet, sizeof(packetStruct));

        if (packet.channel >= CHANNELS) {
          Serial.printf("Illegal channel number %d\n", packet.channel);
          return false;
        }


        // next frame? calc frame delay
        if (packet.frame != lastFrame) {

          // calc delay
          unsigned long now = micros();
          avgDelay = (avgDelay * 0.99) + (now - lastFrameTime) * 0.01;
          lastFrameTime = now;
          lastFrame = packet.frame;
        }

        // update indexes
        recvIndex++;
        if (recvIndex == BUFFER)
          recvIndex = 0;

        if (readIndex == recvIndex) {
          // set to oldest unread packet
          readIndex = recvIndex + 1;
          if (readIndex == BUFFER)
            readIndex = 0;
        }

        // Serial.printf("recv frame %d channel %d, recvindex=%d readindex=%d\n", packet.frame, packet.channel, recvIndex, readIndex);

        return (true);
      }
    }
    return (false);
  }

  packetStruct* readNext()
  {
    int ret = readIndex;
    readIndex++;
    if (readIndex == BUFFER)
      readIndex = 0;

    return (&packets[ret]);
  }

  // current amount of buffered packets available
  byte available()
  {
    int diff = (recvIndex - readIndex);
    if (diff < 0)
      diff = diff + BUFFER;

    return (diff);
  }
};

UdpBuffer udpBuffer = UdpBuffer();

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
  udpBuffer.begin(65000);
  udpBuffer.reset();
  unsigned long lastTime = micros();
  unsigned long now;
  byte lastFrame = 0;

  packetStruct* packet = NULL;
  bool ready = false;


  while (1) {

    // recv packets and calc framerate
    udpBuffer.recvNext();

    // ready to show next?
    if (ready) {
      now = micros();
      if (now - lastTime >= udpBuffer.avgDelay) {
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
          Serial.printf("avail=%d, time=%d\n", udpBuffer.available(), micros()-lastTime);

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
