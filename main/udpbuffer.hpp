
#include <FastLED.h>

//this just stores received udp packets in a circular buffer.


//max number of currentPacket to buffer
#define BUFFER 10

//#define QOIS_DATA_LEN 1472-4
#define QOIS_HEADER_LEN 6
#define QOIS_DATA_LEN (1460 - QOIS_HEADER_LEN)


struct udpPacketStruct {
    uint8_t packetNr;
    [[maybe_unused]] uint8_t reserved1;
    uint16_t time;
    uint16_t syncOffset;
    uint8_t data[QOIS_DATA_LEN]; //contains multiple framestructs, with the first complete one starting at syncOffset
};


class UdpBuffer {
private:
    udpPacketStruct packets[BUFFER];
    uint16_t plens[BUFFER];
    uint8_t readIndex;
    uint8_t recvIndex;
//    bool full;
    uint8_t lastPacketNr;

public:

    //current currentPacket and packetlength. can be null
    udpPacketStruct *currentPacket = nullptr;
    uint16_t currentPlen;


    UdpBuffer() {

        reset();
    }

//    void begin(int port) { udp.begin(port); }

    void reset() {
        readIndex = 0;
        recvIndex = 0;
        lastPacketNr = 0;
//        full = false;
    }

    // receive and store next udp frame if its available.
    // returns received time from packet (used for timesyncronisation)
    // 0=no time
    uint16_t handle(char *data, uint16_t len) {

        if (len) {
            if (full()) {
                ESP_LOGW(TAG, "buffer overflow, packet dropped.");
            } else {
                udpPacketStruct &udpPacket = packets[recvIndex];
                memset(&udpPacket, 0, sizeof(udpPacketStruct));
                memcpy(&udpPacket, data, len);
                plens[recvIndex] = len;

                if (udpPacket.packetNr == lastPacketNr) {
                    ESP_LOGD(TAG, "Dropped duplicate currentPacket.");
                    return (0);
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
                return (udpPacket.time);
            }
        }

        return (0);
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


        currentPacket = &packets[ret];
        currentPlen = plens[ret];
    }


    // current amount of buffered packets available
    uint8_t available() const {

        int diff = (recvIndex - readIndex);
        if (diff < 0)
            diff = diff + BUFFER;

        return (diff);
    }

    bool full() const {
        //1 less because we need to keep oldest currentPacket valid for user
        return (available() == BUFFER - 1);
    }

};
