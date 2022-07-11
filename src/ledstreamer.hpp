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

    Ledstreamer(CRGB * pixels, int px_len) : multicastSync(), udpBuffer(), qois( pixels,  px_len) {
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

        return;

        if (ready) {
            // its time to output the prepared leds buffer?
            if (multicastSync.remoteMillis() >= qois.show_time) {
                FastLED.show();
                // Serial.printf("avail=%d, showtime=%d \n",
                // udpBuffer.available(),showTime);
                ready = false;
            }
        } else {
            //prepare next frame

            //need new packet?
            if (currentPacket == nullptr) {
                if (udpBuffer.available() > 0) {
                    currentPacket = udpBuffer.readNext();
                    //desynced?
//                    if (lastPacketNr+1!=currentPacket->packetNr)
                    if (true) {
                        //abort and start new frame and jump to next syncoffset
                        qois.startFrame();

                        if (currentPacket->syncOffset < QOIS_DATA_LEN)
                            currentByteNr = currentPacket->syncOffset;
                        else {
                            //cant sync on this packet, wait for next one
                            currentPacket= nullptr;
                        }

                    } else {
                        //continue at the beginning of next packet
                        currentByteNr = 0;
                    }
                }
            } else {
                if (currentByteNr < QOIS_DATA_LEN)
                {
                    if (!qois.decodeByte(currentPacket->data[currentByteNr]))
                    {
                        //frame is complete, its showtime :)
                        ready=true;

                    }
                    currentByteNr++;
                }
                else
                {
                    //done with this packet, get next one and continue there
                    currentPacket= nullptr;
                    currentByteNr=0;
                }

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
