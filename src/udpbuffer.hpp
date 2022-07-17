#include <Arduino.h>

#include <FastLED.h>
#include <WiFiUdp.h>


//max number of currentPacket to buffer
#define BUFFER 30

//#define QOIS_DATA_LEN 1472-4
#define QOIS_DATA_LEN 1460-4
#define QOIS_HEADER_LEN 4

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
private:
    udpPacketStruct packets[BUFFER];
    uint16_t plens[BUFFER];
    byte readIndex;
    byte recvIndex;
//    boolean full;
    uint8_t lastPacketNr;
    WiFiUDP udp;

public:

    //current currentPacket and packetlength. can be null
    udpPacketStruct * currentPacket= nullptr;
    uint16_t currentPlen;


    UdpBuffer() {

        reset();
    }

    void begin(int port) { udp.begin(port); }

    void reset() {
        readIndex = 0;
        recvIndex = 0;
        lastPacketNr = 0;
//        full = false;
    }

    // receive and store next udp frame if its available.
    void handle() {
        int plen = udp.parsePacket();

        if (plen) {
            if (full()) {
                ESP_LOGW(TAG, "buffer overflow, packet dropped.");
                udp.flush();
            } else {
                udpPacketStruct &udpPacket = packets[recvIndex];
                memset(&udpPacket, 0, sizeof(udpPacketStruct));
                udp.read((char *) &udpPacket, plen);
                plens[recvIndex]=plen;

                if (udpPacket.packetNr == lastPacketNr) {
                    ESP_LOGD(TAG, "Dropped duplicate currentPacket.");
                    return;
                }


                //drop out of order currentPacket?
                //                if (udpPacket.packetNr!=0 && udpPacket.packetNr < lastPacketNr) {
                //                    ESP_LOGD(TAG, "Dropped out of order currentPacket. (currentPacket nr %d, last was %d) ", udpPacket.packetNr, lastPacketNr);
                //                    return;
                //                }

                const int diff = udpPacket.packetNr - lastPacketNr;
                if (diff > 1)
                    ESP_LOGD(TAG, "Lost %d packets.", diff);


                lastPacketNr = udpPacket.packetNr;

                // update indexes
                recvIndex++;
                if (recvIndex == BUFFER)
                    recvIndex = 0;



                // Serial.printf("handle frame %d channel %d, recvindex=%d
                // readindex=%d\n", udpPacket.frame, udpPacket.channel, recvIndex, readIndex);

            }
        }
    }

    //sets current currentPacket to next avaiable currentPacket, or nullptr if none
     void readNext() {

        if (!available()) {
            currentPacket = nullptr;
            return;
        }

        int ret = readIndex;
        readIndex++;
        if (readIndex == BUFFER)
            readIndex = 0;

//        if (available() == 0)
//            ESP_LOGW(TAG, " Buffer underrun");


        currentPacket=&packets[ret];
        currentPlen=plens[ret];
    }



    // current amount of buffered packets available
    byte available() const {

        int diff = (recvIndex - readIndex);
        if (diff < 0)
            diff = diff + BUFFER;

        return (diff);
    }

    boolean full() const {
        //1 less because we need to keep oldest currentPacket valid for user
        return (available() == BUFFER - 1);
    }

};
