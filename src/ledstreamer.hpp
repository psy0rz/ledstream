//
// Created by psy on 7/11/22.
//

#ifndef LEDSTREAM_LEDSTREAMER_HPP
#define LEDSTREAM_LEDSTREAMER_HPP


#include "multicastsync.hpp"
#include "qois.hpp"
#include "udpbuffer.hpp"

class Ledstreamer {
public:
    MulticastSync multicastSync;

    Ledstreamer(CRGB *pixels, int px_len) : multicastSync(), udpBuffer(), qois(pixels, px_len) {
        ready = false;
        lastPacketNr = 0;
        currentPacket = nullptr;
        currentByteNr = 0;
        synced = false;
    }

    void begin() {
        multicastSync.begin();
        udpBuffer.begin(65000);
        udpBuffer.reset();
    }

    //make sure currentPacket is valid and currentByteNr is still in sync
    boolean packetValid() {

        //already have one
        if (currentPacket != nullptr)
            return true;

        //get new one
        udpPacketStruct *packet = udpBuffer.readNext();
        if (packet == nullptr)
            return false;

        if (synced) {
            //still synced?
            if (lastPacketNr + 1 == packet->packetNr) {
                lastPacketNr = packet->packetNr;
                currentPacket = packet;
                return true;
            } else {
                ESP_LOGW(TAG, "desynced");
                synced = false;
            }
        }

        if (!synced) {
            //can we sync to this packet?
            if (packet->syncOffset < QOIS_DATA_LEN) {
                //reset everything and start from this packet and syncoffset.
                ESP_LOGW(TAG, "synced");
                currentPacket = packet;
                currentByteNr = packet->syncOffset;
                lastPacketNr = packet->packetNr;
                qois.nextFrame();
                synced = true;
                return true;
            }
        }

        return false;
    }

    void handle() {
        multicastSync.handle();
        udpBuffer.handle();

        //leds are ready to be shown?
        if (ready) {
            // its time to output the prepared leds buffer?
//            if (multicastSync.remoteMillis() >= qois.show_time+1200) {
            if (true) {
//                FastLED.show();

                ready = false;
            }
        } else {
            if (packetValid()) {
                //feed available bytes to decoder until we run out, or until it doesnt want any more:
                while (currentByteNr < QOIS_DATA_LEN) {
                    if (!qois.decodeByte(currentPacket->data[currentByteNr])) {
                        ready = true;
                        currentByteNr++;
                        qois.nextFrame();
                        return;
                    }
                    currentByteNr++;
                }
                ESP_LOGD(TAG, "trying to continue in next packet");
                currentByteNr=0;
                currentPacket = nullptr;
            }
        }
    }


private:

    UdpBuffer udpBuffer;
    Qois qois;

    boolean ready;
    boolean synced;
    udpPacketStruct *currentPacket;
    uint8_t lastPacketNr;
    uint16_t currentByteNr;


};


#endif //LEDSTREAM_LEDSTREAMER_HPP
