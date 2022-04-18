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
  // float correctionFactor = 1;
  int correction;

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


        unsigned long correctedLocalDelta=localDelta+correction;
        int diff=remoteDelta-correctedLocalDelta;

        
        Serial.printf("Time packet: diff=%d mS correction=%d mS\n",  diff,  correction);

        // unsigned long calcedRemote=correctedLocalDelta+remoteStartTime;
        // int echtediff=packet.time-calcedRemote;
        // Serial.printf("echte diff: %u - %u  =  %d mS\n", packet.time, calcedRemote, echtediff);

        if (abs(diff)>10000)
        {
          Serial.printf("Reset time. (diff is %d)\n",  diff);
          correction=0;
          localStartTime=now;
          remoteStartTime=packet.time;
        }
        else
        {
          if (diff>0)
            correction++;
          else
            correction--;
          //NOTE: correction factors give jittery rounding errors. so we use a correction offset.
          // correctionFactor = (float)remoteDelta / (float)localDelta;
        }

      }
    }
  }

  unsigned long remoteMillis()
  {
    return (millis()- localStartTime+remoteStartTime+correction);
  }
};
