#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>

struct syncPacketStruct
{
  uint32_t time;
};

class MulticastSync
{
  WiFiUDP udp;
  syncPacketStruct packet;
  // unsigned long lastTime;

  // unsigned long remoteTime;
  // unsigned long localOffset;

  unsigned long localStartTime;
  unsigned long remoteStartTime;
  float correctionFactor = 1;

public:
  void begin()
  {
    udp.beginMulticast(IPAddress(239, 137, 111, 222), 65001);
    // remoteTime = 0;
    // localOffset=0;
  };

  void recv()
  {
    int plen = udp.parsePacket();
    if (plen) {
      if (plen != sizeof(syncPacketStruct)) {
        Serial.printf("Ignored sync packet with length %d (should be %d)\n", plen, sizeof(syncPacketStruct));
        udp.flush();

      } else {
        unsigned long now = millis();
        udp.read((char*)&packet, sizeof(syncPacketStruct));

        unsigned long localDelta=now-localStartTime;
        unsigned long remoteDelta=packet.time-remoteStartTime;


        unsigned long correctedLocalDelta=localDelta*correctionFactor;
        int diff=remoteDelta-correctedLocalDelta;

        
        Serial.printf("Time packet: diff=%d mS correctionfactor=%f\n",  diff,  correctionFactor);

        // unsigned long calcedRemote=correctedLocalDelta+remoteStartTime;
        // int echtediff=packet.time-calcedRemote;
        // Serial.printf("echte diff: %u - %u  =  %d mS\n", packet.time, calcedRemote, echtediff);

        if (abs(diff)>10000)
        {
          Serial.printf("Reset time. (diff is %d)\n",  diff);
          correctionFactor=1;
          localStartTime=now;
          remoteStartTime=packet.time;
        }
        else
        {
          correctionFactor = (float)remoteDelta / (float)localDelta;
        }

      }
    }
  }

  unsigned long remoteMillis()
  {
    return (((millis()- localStartTime)*correctionFactor)+remoteStartTime);
  }
};
