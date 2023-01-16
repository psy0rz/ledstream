//
// Created by psy on 15-1-23.
//

#ifndef LEDDER_UDPSERVER_HPP
#define LEDDER_UDPSERVER_HPP


#include <lwip/sockets.h>

static const char *UDPSERVER_TAG = "udpserver";

class UdpServer {

    int sock;

public:
    UdpServer() {


    }

    void begin(uint16_t  port)
    {
        //create listen socket on adres/port
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr= {
                .sin_family = AF_INET,
                .sin_port=htons(port),
                .sin_addr={
                        .s_addr=htonl(INADDR_ANY)
                },

        };

        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            ESP_LOGE(UDPSERVER_TAG, "Unable to create socket: errno %d", errno);
            return;

        }
        ESP_LOGI(UDPSERVER_TAG, "Socket created");

        //actually bind socket
        int err = bind(sock, (struct sockaddr *) &dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(UDPSERVER_TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(UDPSERVER_TAG, "Socket bound, port %d", port);


    }

    //recv next packet to buffer and returns number of bytes received.
    ssize_t process(void *buffer, uint16_t len) const {



        return recv(sock, buffer, len, MSG_DONTWAIT);

    }

};


#endif //LEDDER_UDPSERVER_HPP
