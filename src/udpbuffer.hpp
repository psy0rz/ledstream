#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>

#ifndef LEDS_PER_CHAN
#define LEDS_PER_CHAN 300
#endif

#ifndef CHANNELS
#define CHANNELS 2
#endif

#define LED_COUNT LEDS_PER_CHAN * CHANNELS

// nr of frames to buffer. marquee tries to keep buffer at 50% to decrease
// jitter
#ifndef BUFFER_FRAMES
#define BUFFER_FRAMES 25
#endif

#define BUFFER BUFFER_FRAMES
//lag at 60fps will be so that the buffer will be at 80%
#define LAG BUFFER_FRAMES*0.8*16

#define DATA_LEN 1000

struct frameStruct
{
  uint32_t time;
  uint8_t data[DATA_LEN];
};

struct udpPacketStruct
{
    int plen; //frame length
    struct frameStruct frame;
};

// circular udp frame buffer
class UdpBuffer
{
  public:
  udpPacketStruct packets[BUFFER];
  byte readIndex;
  byte recvIndex;


  WiFiUDP udp;

  UdpBuffer() {
      reset();
  }

  void begin(int port) { udp.begin(port); }

  void reset()
  {
    readIndex = 0;
    recvIndex = 0;
  }

  // receive and store next udp frame if its available.
  // returns true if new frame
  bool recvNext()
  {
    int plen = udp.parsePacket();

    if (plen) {
      if (plen > sizeof(frameStruct)) {
        Serial.printf("udpbuffer: Packet length %d too big (expected %d)\n", plen, sizeof(frameStruct));
        udp.flush();
      } else {
        udpPacketStruct& udpPacket = packets[recvIndex];


        // read and store udpPacket
//        memset((char*)&udpPacket.frame, 0, sizeof(frameStruct));
        udp.read((char*)&udpPacket.frame, sizeof(frameStruct));
        udpPacket.plen=plen;

        // update indexes
        recvIndex++;
        if (recvIndex == BUFFER)
          recvIndex = 0;

        if (readIndex == recvIndex) {
          Serial.println("udpbuffer: Buffer overrun");
          //shouldnt happen normally..just clear
          reset();
          return(false);
        }

        // Serial.printf("recv frame %d channel %d, recvindex=%d
        // readindex=%d\n", udpPacket.frame, udpPacket.channel, recvIndex, readIndex);

        return (true);
      }
    }
    return (false);
  }



  udpPacketStruct* readNext()
  {
    int ret = readIndex;
    readIndex++;
    if (readIndex == BUFFER)
      readIndex = 0;

    if (available()==0)
        Serial.println("udpbuffer: Buffer underrun");


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
