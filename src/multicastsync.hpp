#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>
#include "utils.hpp"

struct syncPacketStruct {
    uint32_t time;
};

class MulticastSync {
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
    void begin() {

        udp.beginMulticast(IPAddress(MCAST_GROUP), 65001);
        // remoteTime = 0;
        // localOffset=0;
        startup = 10;
    };

    void update()
    {

    }

    void handle() {


        int plen = udp.parsePacket();
        if (plen) {
            if (plen != sizeof(syncPacketStruct)) {
                ESP_LOGE(TAG, "Ignored sync frame with length %d (should be %d)", plen, sizeof(syncPacketStruct));
                udp.flush();

            } else {
                unsigned long now = millis();
                lastRecvTime = now;

                udp.read((char *) &packet, sizeof(syncPacketStruct));

                unsigned long localDelta = now - localStartTime;
                unsigned long remoteDelta = packet.time - remoteStartTime;

                unsigned long correctedLocalDelta = localDelta + correction;
                int diff = diffUnsignedLong(remoteDelta, correctedLocalDelta);

                if (startup) {
                    correction = 0;
                    localStartTime = now;
                    remoteStartTime = packet.time;
                    startup = startup - 1;
                    ESP_LOGD(TAG, "starting %d", startup);
                } else {
                    ESP_LOGD(TAG, "diff=%d mS correction=%d mS, time=%u mS", diff, correction, remoteMillis16());

//                    if (abs(diff) > 1000 || !synced()) {
                        if ( !synced()) {
                        ESP_LOGW(TAG, "Desynced, restarting");
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

    unsigned long remoteMillis() const {
        return (millis() - localStartTime + remoteStartTime + correction);
    }

    uint16_t remoteMillis16() const {
        return ((millis() - localStartTime + remoteStartTime + correction));
    }

    bool synced() const {
        if (millis() - lastRecvTime < 3000)
            return true;
        else
            return false;
    }
};
