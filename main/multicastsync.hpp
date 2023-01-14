
#include "utils.hpp"
#include <FastLED.h>

// This class tries to sync the local time against the remote time.
// It does this by looking at the remotetime that was received by either
// multicast or the last packet. remoteMillis() should return the same time as
// the remotehost.

// We use small corrections to filter out the network jitter.

struct syncPacketStruct
{
  uint32_t time;
};

class MulticastSync
{
  // unsigned long lastTime;

  // unsigned long remoteTime;
  // unsigned long localOffset;

  uint16_t localStartTime;
  uint16_t remoteStartTime;
  uint16_t lastRecvTime;
  unsigned long lastDebugOutput;
  // float correctionFactor = 1;
  int correction;
  uint8_t startup;

  public:

  void begin()
  {

    lastDebugOutput = 0;
    // remoteTime = 0;
    // localOffset=0;
    startup = 10;
  };


  // can either use specified time, or time received via multicast.
  // use time=0 to only use multicast
  void handle(uint16_t syncTime)
  {

    // syncTime is the one that will be used. It can either get filled by
    // multicast or via lastPacketTime


    if (syncTime) {
      uint16_t now = esp_timer_get_time()/1000;
      lastRecvTime = now;
      uint16_t localDelta = now - localStartTime;
      uint16_t remoteDelta = syncTime - remoteStartTime;

      uint16_t correctedLocalDelta = localDelta + correction;
      int diff = diff16(remoteDelta, correctedLocalDelta);

      if (startup) {
        correction = 0;
        localStartTime = now;
        remoteStartTime = syncTime;
        startup = startup - 1;
        ESP_LOGD(TAG, "starting %d", startup);
      } else {

        if ( (millis() - lastDebugOutput >= 500)) {
          ESP_LOGD(TAG,
                   "received=%d mS, remoteMillis=%u mS, "
                   "correction=%d, diff=%d",
                   syncTime,
                   remoteMillis(),
                   correction,
                   diff);
          lastDebugOutput = millis();
        }

        if (!synced()) {
          ESP_LOGW(TAG, "Desynced, restarting");
          startup = 10;

        } else {

          // NOTE: correction factors give jittery rounding errors. so we
          // use a correction offset. correctionFactor = (float)remoteDelta
          // / (float)localDelta;
          if (diff > 0)
            correction++;
          else
            correction--;
        }
      }
    }
  }

  //   unsigned long remoteMillis() const
  //   {
  //     return (millis() - localStartTime + remoteStartTime + correction);
  //   }

  uint16_t remoteMillis() const
  {
    return ((millis() - localStartTime + remoteStartTime + correction));
  }

  bool synced() const
  {
    if (diff16(millis(), lastRecvTime) < 3000)
      return true;
    else
      return false;
  }
};
