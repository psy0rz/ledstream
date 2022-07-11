#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>

#ifndef LEDS_PER_CHAN
#define LEDS_PER_CHAN 300
#endif

#ifndef CHANNELS
#define CHANNELS 2
#endif

#define LED_COUNT LEDS_PER_CHAN * CHANNELS

// nr of frames to buffer. marquee tries to keep buffer at 50% to decrease
// jitter
#ifndef BUFFER_FRAMES
#define BUFFER_FRAMES 60
#endif

#define BUFFER 10
//lag at 60fps will be so that the buffer will be at 80%
#define LAG BUFFER_FRAMES*0.8*16

//#define QOIS_DATA_LEN 1472-4
#define QOIS_DATA_LEN 1460-4

//struct frameStruct {
//    uint16_t frameLength;
//    uint32_t time;
//    //data...
//};

struct udpPacketStruct {
    uint8_t packetNr;
    uint8_t reserved1;
    uint16_t syncOffset;
    uint8_t data[QOIS_DATA_LEN]; //contains multiple framestructs, with the first complete one starting at syncOffset
};


// circular udp frame buffer
class UdpBuffer {
public:
    udpPacketStruct packets[BUFFER];
    byte readIndex;
    byte recvIndex;
    uint8_t lastPacketNr;

    WiFiUDP udp;

    UdpBuffer() {
        reset();
    }

    void begin(int port) { udp.begin(port); }

    void reset() {
        readIndex = 0;
        recvIndex = 0;
        lastPacketNr = 0;
    }

    // receive and store next udp frame if its available.
    void handle() {
        int plen = udp.parsePacket();

        if (plen) {
            if (plen != sizeof(udpPacketStruct)) {
                Serial.printf("udpbuffer: Incorrect packet length:  %d (expected %d)\n", plen, sizeof(udpPacketStruct));
                udp.flush();
            } else {
                udpPacketStruct &udpPacket = packets[recvIndex];
                udp.read((char *) &udpPacket, plen);

                if (udpPacket.packetNr == lastPacketNr) {
                    Serial.printf("udpbuffer: Dropped duplicate packet.\n");
                    return;
                }


                //drop out of order packet?
                if (udpPacket.packetNr!=0 && udpPacket.packetNr < lastPacketNr) {
                    Serial.printf("udpbuffer: Dropped out of order packet. (packet nr %d, last was %d) \n", udpPacket.packetNr, lastPacketNr);
                    return;
                }

                const int diff=udpPacket.packetNr-lastPacketNr;
                if (diff>1)
                    Serial.printf("udpbuffer: Lost %d packets.\n", diff);


                lastPacketNr=udpPacket.packetNr;

                // update indexes
                recvIndex++;
                if (recvIndex == BUFFER)
                    recvIndex = 0;

                if (readIndex == recvIndex) {
                    Serial.println("udpbuffer: Buffer overrun");
                    //shouldnt happen normally..just clear
                    reset();
                    return;
                }

                // Serial.printf("handle frame %d channel %d, recvindex=%d
                // readindex=%d\n", udpPacket.frame, udpPacket.channel, recvIndex, readIndex);

                return;
            }
        }

    }


    udpPacketStruct *readNext() {
        int ret = readIndex;
        readIndex++;
        if (readIndex == BUFFER)
            readIndex = 0;

        if (available() == 0)
            Serial.println("udpbuffer: Buffer underrun");


        return (&packets[ret]);
    }

    // current amount of buffered packets available
    byte available() const {
        int diff = (recvIndex - readIndex);
        if (diff < 0)
            diff = diff + BUFFER;

        return (diff);
    }
};
