#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>

struct syncPacketStruct
{
  uint16_t interval;
};

class MulticastSync
{
  WiFiUDP udp;
  syncPacketStruct packet;
  unsigned long lastTime;

  unsigned long remoteTime;
  unsigned long startTime;
  float correctionFactor=1;

public:
  void begin()
  {
    udp.beginMulticast(IPAddress(239, 137, 111, 222), 65001);
    remoteTime = 0;
  };

  void recv()
  {
    int plen = udp.parsePacket();
    if (plen) {
      if (plen != sizeof(syncPacketStruct)) {
        Serial.printf("Ignored sync packet with length %d\n", plen);
        udp.flush();

      } else {
        unsigned long now = micros();
        udp.read((char*)&packet, sizeof(syncPacketStruct));


        // error in uS
        // int error = diff - (int)(packet.interval * 1000);

        if (remoteTime==0)
        {
            startTime=now-1000000;
        }

        remoteTime = remoteTime + packet.interval*1000;
        unsigned long localTime=(now-startTime);


        unsigned long guessedRemoteTime=localTime*correctionFactor;
        unsigned long diff=guessedRemoteTime-remoteTime;

        correctionFactor=(float)remoteTime/(float)localTime;


        Serial.printf(
          "Sync packet %d mS, diff= %d uS, correction=%f\n", packet.interval, diff, correctionFactor);
      }
    }
  };
};
