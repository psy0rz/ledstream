
#include "utils.hpp"
#include <FastLED.h>

// This class tries to sync the local time against the remote time.
// It does this by looking at the remotetime that was received by either
// multicast or the last packet. remoteMillis() should return the same time as
// the remotehost.

// We use small corrections to filter out the network jitter.
static const char *TIMESYNCTAG = "timesync";

//step sync our internal time if difference is too much for too long
#define MAX_DIFF_MS 10
#define RESYNC_TIME_MS 1000


class TimeSync {

    uint16_t localStartTime;
    uint16_t remoteStartTime;
    unsigned long lastDebugOutput;
    int correction;

    uint16_t desyncStartTime;
    bool desynced;

public:

    void begin() {
        lastDebugOutput = 0;
        desynced = true;
        desyncStartTime=-10000;
        localStartTime=0;
        remoteStartTime=0;
        correction=0;
        lastDebugOutput=0;
    };

    // can either use specified time, or time received via multicast.
    // use time=0 to only use multicast
    void process(uint16_t syncTime) {

        uint16_t now = millis();
        uint16_t localDelta = now - localStartTime;
        uint16_t remoteDelta = syncTime - remoteStartTime;

        uint16_t correctedLocalDelta = localDelta + correction;
        int diff = diff16(remoteDelta, correctedLocalDelta);

        //diff too big?
        if (abs(diff) > MAX_DIFF_MS) {
            if (!desynced) {
                desynced = true;
                desyncStartTime = now;
            } else {
                int desyncLength = diff16(now, desyncStartTime);
                if (desyncLength > RESYNC_TIME_MS) {
                    correction = 0;
                    localStartTime = now;
                    remoteStartTime = syncTime;
                    ESP_LOGW(TIMESYNCTAG, "resynced timing (diff was %d mS for %d mS)", abs(diff), desyncLength);
                }
            }
        } else {
            desynced = false;
            //do clock corrections by 1 mS steps
            if (diff > 0)
                correction++;
            else if (diff < 0)
                correction--;
        }

        if ((now - lastDebugOutput >= 1000)) {
            ESP_LOGI(TIMESYNCTAG,
                     "received=%d mS, remoteMillis=%u mS, "
                     "correction=%d, diff=%d",
                     syncTime,
                     remoteMillis(),
                     correction,
                     diff);
            lastDebugOutput = now;
        }

    }

    uint16_t remoteMillis() const {
        return ((millis() - localStartTime + remoteStartTime + correction));
    }

};
