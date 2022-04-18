#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>

#define NUM_LEDS 300
#define CHANNELS 2

// nr of frames to buffer. marquee tries to keep buffer at 50% to decrease
// jitter
#define BUFFER_FRAMES 25
#define BUFFER BUFFER_FRAMES* CHANNELS

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

  UdpBuffer() {}

  void begin(int port) { udp.begin(port); }

  void reset()
  {
    readIndex = 0;
    recvIndex = 0;
    lastFrame = 0;
  }

  // receive and store next udp packet if its available.
  // returns true if new packet
  bool recvNext()
  {
    int plen = udp.parsePacket();
    if (plen) {
      if (plen != sizeof(packetStruct)) {
        Serial.printf("Ignored data packet with length %d\n", plen);
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

          lastFrame = packet.frame;
          lastFrameTime = micros();
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

        // Serial.printf("recv frame %d channel %d, recvindex=%d
        // readindex=%d\n", packet.frame, packet.channel, recvIndex, readIndex);

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
