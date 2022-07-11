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
        currentByteNr = UINT16_MAX;
    }

    void begin() {
        multicastSync.begin();
        udpBuffer.begin(65000);
        udpBuffer.reset();
    }

    void handle() {
        multicastSync.handle();
        udpBuffer.handle();

//        udpBuffer.readNext();

        if (ready) {
            // its time to output the prepared leds buffer?
//            if (multicastSync.remoteMillis() >= qois.show_time+1200) {
            if (true) {
                FastLED.show();
                qois.startFrame();

                // Serial.printf("avail=%d, showtime=%d \n",
                // udpBuffer.available(),showTime);
                ready = false;

                if (currentPacket->data[currentByteNr]!=0xff) {
                    ESP_LOGE(TAG, "next byte wasnt sync?");
                    currentPacket = nullptr;
                    currentByteNr = UINT16_MAX;
                }
                currentByteNr++;


                }
        } else {
            //prepare next frame

            //need new packet?
            if (currentPacket == nullptr) {
                if (udpBuffer.available() > 0) {
                    currentPacket = udpBuffer.readNext();
//                    ESP_LOGD(TAG, "new packet, nr %d", currentPacket->packetNr);
                    //desynced?
                    if (lastPacketNr + 1 != currentPacket->packetNr)
                        ESP_LOGW(TAG,"missed packet");

                    if (lastPacketNr + 1 != currentPacket->packetNr || currentByteNr == UINT16_MAX) {
                        ESP_LOGD(TAG, "desynced");
                        //abort and start new frame and jump to next syncoffset

                        if (currentPacket->syncOffset < QOIS_DATA_LEN) {
                            currentByteNr = currentPacket->syncOffset;
                            qois.startFrame();
                            if (currentPacket->data[currentByteNr]!=0xff)
                            {
                                ESP_LOGE(TAG, "sync byte wasnt 0xff??");
                                currentPacket = nullptr;
                                currentByteNr = UINT16_MAX;
                            }
                            currentByteNr++;
                        } else {
                            //cant sync on this packet, wait for next one
                            currentPacket = nullptr;
                            currentByteNr = UINT16_MAX;
                        }

                    } else {
                        //continue at the beginning of this packet
                        ESP_LOGD(TAG, "continuing in next packet");
                        currentByteNr = 0;
                    }
                    lastPacketNr = currentPacket->packetNr;
                }
            } else {
                while (currentByteNr < QOIS_DATA_LEN) {
                    if (!qois.decodeByte(currentPacket->data[currentByteNr])) {
                        ready = true;
                        currentByteNr++;
                        return;
                    }
                    currentByteNr++;
                }
                //done with this packet, get new one
                ESP_LOGD(TAG, "trying to continue in next packet");
                currentPacket = nullptr;

            }
        }
    }


private:

    UdpBuffer udpBuffer;
    Qois qois;

    boolean ready;
    udpPacketStruct *currentPacket;
    uint8_t lastPacketNr;
    uint16_t currentByteNr;


};


#endif //LEDSTREAM_LEDSTREAMER_HPP
