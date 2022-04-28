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
  unsigned long lastRecvTime;
  // float correctionFactor = 1;
  int correction;
  byte startup;

public:
  void begin()
  {
    udp.beginMulticast(IPAddress(239, 137, 111, 222), 65001);
    // remoteTime = 0;
    // localOffset=0;
    startup = 10;
  };

  void recv()
  {
    int plen = udp.parsePacket();
    if (plen) {
      if (plen != sizeof(syncPacketStruct)) {
        Serial.printf("Ignored sync packet with length %d (should be %d)\n",
                      plen,
                      sizeof(syncPacketStruct));
        udp.flush();

      } else {
        unsigned long now = millis();
        lastRecvTime = now;

        udp.read((char*)&packet, sizeof(syncPacketStruct));

        unsigned long localDelta = now - localStartTime;
        unsigned long remoteDelta = packet.time - remoteStartTime;

        unsigned long correctedLocalDelta = localDelta + correction;
        int diff = remoteDelta - correctedLocalDelta;

        if (startup) {
          correction = 0;
          localStartTime = now;
          remoteStartTime = packet.time;
          startup = startup - 1;
          Serial.printf("timesync: starting %d\n", startup);
        } else {
          Serial.printf(
            "timesync: diff=%d mS correction=%d mS\n", diff, correction);

          if (abs(diff) > 10000 || !synced()) {
            Serial.printf(
              "timesync: Desynced, restarting\n");
            startup = 10;

          } else {

            // NOTE: correction factors give jittery rounding errors. so we use
            // a correction offset. correctionFactor = (float)remoteDelta /
            // (float)localDelta;
            if (diff > 0)
              correction++;
            else
              correction--;
          }
        }
      }
    }
  }

  unsigned long remoteMillis()
  {
    return (millis() - localStartTime + remoteStartTime + correction);
  }

  bool synced()
  {
    if (millis() - lastRecvTime < 3000)
      return true;
    else
      return false;
  }
};
