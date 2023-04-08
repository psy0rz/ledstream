
#ifndef LEDDER_UDPBUFFER_HPP
#define LEDDER_UDPBUFFER_HPP

static const char *UDPBUFFER_TAG = "udpbuffer";



//this just stores received udp packets in a circular buffer.


//max number of currentPacket to buffer


//#define QOIS_DATA_LEN 1472-4
#define QOIS_HEADER_LEN 6
#define QOIS_DATA_LEN (1460 - QOIS_HEADER_LEN)


struct udpPacketStruct {
    uint8_t packetNr;
    uint8_t reserved1;
    uint16_t time;
    uint16_t syncOffset;
    uint8_t data[QOIS_DATA_LEN]; //contains multiple framestructs, with the first complete one starting at syncOffset
};

auto const udpPacketSize = sizeof(udpPacketStruct);


class UdpBuffer {
private:
    udpPacketStruct packets[CONFIG_LEDSTREAM_UDP_BUFFERS];
    uint16_t plens[CONFIG_LEDSTREAM_UDP_BUFFERS];
    uint8_t readIndex;
    uint8_t recvIndex;
//    bool full;
    uint8_t lastPacketNr;

public:

    //current currentPacket and packetlength. can be null
    udpPacketStruct *currentPacket = nullptr;
    uint16_t currentPlen=0;
    uint16_t lastStatsTime=0;


    UdpBuffer() {
        readIndex = 0;
        recvIndex = 0;
        lastPacketNr = 0;

    }

//    void begin(int port) { udp.begin(port); }

    void reset() {
        readIndex = 0;
        recvIndex = 0;
        lastPacketNr = 0;
//        full = false;
    }

    //get pointer to next recv buffer thats ready to be filled, or null if we have now more buffers left
    udpPacketStruct  * getRecvBuffer() {
        if (full())
            return nullptr;

        return &packets[recvIndex];

    }

    // process the data the just got received in the next getRecvBuffer()
    // returns received time from packet (used for timesyncronisation)
    // 0=no time/dropped packet
    uint16_t process(uint16_t len) {

        uint16_t now=millis();
        if (diff16(now, lastStatsTime)>1000)
        {
            lastStatsTime=now;
            ESP_LOGI(UDPBUFFER_TAG, "buffered %d/%d", available(), CONFIG_LEDSTREAM_UDP_BUFFERS);
        }


        if (len) {

                udpPacketStruct &udpPacket = packets[recvIndex];
//                memset(&udpPacket, 0, sizeof(udpPacketStruct));
//                memcpy(&udpPacket, data, len);
                plens[recvIndex] = len;



                if (udpPacket.packetNr == lastPacketNr) {
                    ESP_LOGW(UDPBUFFER_TAG, "Dropped duplicate currentPacket.");
                    return (0);
                }
                //drop out of order currentPacket?
                //                if (udpPacket.packetNr!=0 && udpPacket.packetNr < lastPacketNr) {
                //                    ESP_LOGD(UDPBUFFER_TAG, "Dropped out of order currentPacket. (currentPacket nr %d, last was %d) ", udpPacket.packetNr, lastPacketNr);
                //                    return;
                //                }

                const int diff = udpPacket.packetNr - lastPacketNr;
                if (diff > 1)
                    ESP_LOGW(UDPBUFFER_TAG, "Lost %d packets.", diff-1);

                lastPacketNr = udpPacket.packetNr;

                // update indexes
                recvIndex++;
                if (recvIndex == CONFIG_LEDSTREAM_UDP_BUFFERS)
                    recvIndex = 0;

                if (full())
                {
                    ESP_LOGW(UDPBUFFER_TAG, "buffer overflow");
                }

                // Serial.printf("process frame %d channel %d, recvindex=%d
                // readindex=%d\n", udpPacket.frame, udpPacket.channel, recvIndex, readIndex);
                return (udpPacket.time);
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
        if (readIndex == CONFIG_LEDSTREAM_UDP_BUFFERS)
            readIndex = 0;

//        if (available() == 0)
//            ESP_LOGW(UDPBUFFER_TAG, " Buffer underrun");


        currentPacket = &packets[ret];
        currentPlen = plens[ret];
    }


    // current amount of buffered packets available
    uint8_t available() const {

        int diff = (recvIndex - readIndex);
        if (diff < 0)
            diff = diff + CONFIG_LEDSTREAM_UDP_BUFFERS;

        return (diff);
    }

    bool full() const {
        //1 less because we need to keep oldest currentPacket valid for user
        return (available() == CONFIG_LEDSTREAM_UDP_BUFFERS - 1);
    }

};

#endif
