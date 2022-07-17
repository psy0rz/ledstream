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
        if (udpBuffer.currentPacket != nullptr)
            return true;

        //try to get new one
        udpBuffer.readNext();
        if (udpBuffer.currentPacket == nullptr)
            return false;


        if (synced) {
            //still synced?
            lastPacketNr++;
            if (lastPacketNr == udpBuffer.currentPacket->packetNr) {
                return true;
            } else {
                ESP_LOGW(TAG, "desynced by lost packet");
                synced = false;
            }
        }

        if (!synced) {
            //can we sync to this currentPacket?
            if (udpBuffer.currentPacket->syncOffset < QOIS_DATA_LEN) {
                //reset everything and start from this currentPacket and syncoffset.
                ESP_LOGW(TAG, "synced");
                currentByteNr = udpBuffer.currentPacket->syncOffset;
                lastPacketNr = udpBuffer.currentPacket->packetNr;
                qois.nextFrame();
                synced = true;
                return true;
            }
        }

        udpBuffer.currentPacket = nullptr;
        return false;
    }

    bool idle() const {
        return ( abs(diff16( multicastSync.remoteMillis16(), qois.show_time))>1000);
    }

    void handle() {
        multicastSync.handle();
        udpBuffer.handle();

        //leds are ready to be shown?
        if (ready) {
            // its time to output the prepared leds buffer?
            if (multicastSync.remoteMillis16() >= qois.show_time) {
//            if (true) {
//                ESP_LOGD(TAG, "remotems=%u showtime=%u\n", multicastSync.remoteMillis16(), qois.show_time);
                FastLED.show();

                ready = false;
            }
        } else {
            if (packetValid()) {
                //feed available bytes to decoder until we run out, or until it doesnt want any more:
//                ESP_LOGD(TAG, "currentbytenr %d", currentByteNr);
//                for (int i = -10; i < 0; i++) {
//                    Serial.print(currentPacket->data[currentByteNr + i], HEX);
//                    Serial.print(" ");
//                }
//
//                Serial.printf("(%d)", currentByteNr);
//
//                for (int i = 0; i < 10; i++) {
//                    Serial.print(currentPacket->data[currentByteNr + i], HEX);
//                    Serial.print(" ");
//                }
//
//                Serial.println();

                while (currentByteNr < udpBuffer.currentPlen - QOIS_HEADER_LEN) {
                    if (!qois.decodeByte(udpBuffer.currentPacket->data[currentByteNr])) {
                        ready = true;
//                        currentByteNr++;
                        qois.nextFrame();
                        return;
                    }
                    currentByteNr++;
                }
//                ESP_LOGD(TAG, "trying to continue in next currentPacket");
                currentByteNr = 0;
                udpBuffer.currentPacket = nullptr;
            }
        }
    }


private:

    UdpBuffer udpBuffer;
    Qois qois;

    boolean ready;
    boolean synced;
    uint8_t lastPacketNr;
    uint16_t currentByteNr;


};


#endif //LEDSTREAM_LEDSTREAMER_HPP
