//
// Created by psy on 7/11/22.
//

#ifndef LEDSTREAM_LEDSTREAMER_HPP
#define LEDSTREAM_LEDSTREAMER_HPP

#include "timesync.hpp"
#include "qois.hpp"
#include "udpbuffer.hpp"
#include "udpserver.hpp"
#include "fileserver.hpp"
#include "leds.hpp"

static const char *LEDSTREAMER_TAG = "ledstreamer";

class Ledstreamer {
private:
    UdpServer udpServer;
    UdpBuffer udpBuffer;
    Qois qois;

    bool ready;
    bool synced;
    uint8_t lastPacketNr;
    uint16_t currentByteNr;

    //we start in static playback mode. if we receive an udp packet we switch to streaming and stay there.
    bool streaming;


public:
    TimeSync timeSync;

    Ledstreamer()
            : udpServer(), udpBuffer(), qois(), timeSync() {
        ready = false;
        lastPacketNr = 0;
        currentByteNr = 0;
        synced = false;
        streaming = false;
    }

    void begin(uint16_t port) {
        qois.nextFrame();
        timeSync.begin();
        udpBuffer.reset();
        udpServer.begin(port);
    }

    // make sure currentPacket is valid and currentByteNr is still in sync
    bool packetValid() {

        // already have one
        if (udpBuffer.currentPacket != nullptr)
            return true;

        // try to get new one
        udpBuffer.readNext();
        if (udpBuffer.currentPacket == nullptr)
            return false;

//        ESP_LOGD(LEDSTREAMER_TAG, "packetnr %d, packet size %d", udpBuffer.currentPacket->packetNr,
//                 udpBuffer.currentPlen);

        if (synced) {
            // still synced?
            lastPacketNr++;

            if (lastPacketNr == udpBuffer.currentPacket->packetNr) {
                return true;
            } else {
                ESP_LOGW(LEDSTREAMER_TAG, "desynced by lost packet");
                synced = false;
            }
        }

        if (!synced) {
            // can we sync to this currentPacket?
            if (udpBuffer.currentPacket->syncOffset < UDP_DATA_LEN) {
                // reset everything and start from this currentPacket and syncoffset.
                ESP_LOGW(LEDSTREAMER_TAG, "synced");
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
        return (abs(diff16(timeSync.remoteMillis(), qois.show_time)) > 1000);
    }

    //process receiving udp packets and updating the ledstrip
    void process() {

        wait_download();

        //read and store udp packets, update time according to received packets (for streaming mode)
        auto udpPacket = udpBuffer.getRecvBuffer();
        if (udpPacket != nullptr) {
            auto packetLen = udpServer.process(udpPacket, udpPacketSize);

            if (packetLen > 0) {
                streaming = true;
//                ESP_LOGD(LEDSTREAMER_TAG, "packet %d bytes", packetLen);

                uint16_t time = udpBuffer.process(packetLen);

                if (time)
                    timeSync.process(time);
            }
        }

        // leds are ready to be shown?
        if (ready) {
            // its time to output the prepared leds buffer? (or is the time too far in the future?)
            auto diff = diff16(timeSync.remoteMillis(), qois.show_time);
            if (diff >= 0 || diff < -1000) {
                leds_show();
                ready = false;
                qois.nextFrame();
            }
        } else {
            //create new leds to be shown

            //streaming UDP mode:
            if (streaming) {

                if (packetValid()) {
                    // feed available bytes to decoder until we run out, or until it doesnt
                    // want any more:
                    while (currentByteNr < udpBuffer.currentPlen - UDP_HEADER_LEN) {
                        const auto wantsMore = qois.decodeByte(udpBuffer.currentPacket->data[currentByteNr]);
                        currentByteNr++;
                        if (!wantsMore) {
                            //frame is complete frame, we're ready to show the leds.
                            ready = true;
                            return;
                        }
                    }
                    //continue in next packet..
                    currentByteNr = 0;
                    udpBuffer.currentPacket = nullptr;
                }
            }
                //static replay mode:
            else {
                //are we at the start of the file?
                if (fileserver_read_offset == 0) {
                    //restart qois decoder and timing
                    qois.nextFrame();
                    timeSync.reset();
                    leds_reset();
                }

                //read a full frame
                while (qois.decodeByte(fileserver_read_next()) ) {
                    //happens when corrupt/downloading
                    if (fileserver_read_offset==0)
                        return;

                };
                ready = true;
            }
        }

    }

};


#endif // LEDSTREAM_LEDSTREAMER_HPP
