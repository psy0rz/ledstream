//
// Created by psy on 7/10/22.
//

#ifndef LEDSTREAM_QOIS_HPP
#define LEDSTREAM_QOIS_HPP

#include <FastLED.h>

#define QOI_ZEROARR(a) memset((a),0,sizeof(a))

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOI_OP_RGBA   0xff /* 11111111 */

#define QOI_MASK_2    0xc0 /* 11000000 */
#define QOI_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)


typedef union {
    struct {
        unsigned char r, g, b, a;
    } rgba;
    unsigned int v;
} qoi_rgba_t;


//byte based QOI stream decoder
class Qois {

    qoi_rgba_t index[64];
    qoi_rgba_t px;
    int px_pos;
    unsigned char bytes[4];
    int bytes_needed;
    int bytes_received;
    int op;
    boolean wait_for_op;

    CRGB *pixels;
    int px_len;
    boolean wait_for_show_time;

public:
    uint32_t show_time;


    Qois(CRGB *pixels, int px_len) {
        this->px_len = px_len;
        this->pixels = pixels;
        QOI_ZEROARR(index);
        startFrame();
    }

    //start decoding a new frame
    void startFrame() {
//        ESP_LOGD(TAG, "start frame");

        px.rgba.r = 0;
        px.rgba.g = 0;
        px.rgba.b = 0;
        px.rgba.a = 1;
        px_pos = 0;

        //stuff added to make QOI byte based
        wait_for_op = false;
        wait_for_show_time = true;
        bytes_needed = 4;
        bytes_received = 0;


    }

    //returns true when we need more data. false means frame is complete
    bool decodeByte(uint8_t data) {

//        ESP_LOGD(TAG, "decode byte: data=%d waitshowtime=%d, waitop=%d, bytes_needed=%d, op=%d", data,wait_for_show_time, wait_for_op, bytes_needed, op);


        //wait for next operation
        if (wait_for_op) {
            op = data;
            bytes_received = 0;
            wait_for_op = false;

            if (op == QOI_OP_RGB)
                bytes_needed = 3;
            else if (op == QOI_OP_LUMA)
                bytes_needed = 1;
            else
                bytes_needed = 0;

            if (bytes_needed)
                return true;
        }

        //store received bytes
        if (bytes_needed) {
            bytes_needed--;
            bytes[bytes_received] = data;
            bytes_received++;

            //still need more?
            if (bytes_needed)
                return true;
        }

        //store show time?
        if (wait_for_show_time) {
            wait_for_show_time = false;
            wait_for_op = true;
            show_time = *(uint32_t *) bytes;

            if ((show_time & 0xEE000000) != 0xEE000000) {
                ESP_LOGE(TAG, "Got wrong time: %d", op);
                Serial.println(show_time, HEX);
                startFrame();
                return true;

            }

//            ESP_LOGD(TAG, "got bytes %d %d", bytes[0], bytes[3]);
//            ESP_LOGD(TAG, "got showtime %u", show_time);
//            Serial.println(show_time, HEX);
            return true;
        }


        //from this point on we know the operation and we have all the bytes we need for QOI

        if (op == QOI_OP_RGB) {
            px.rgba.r = bytes[0];
            px.rgba.g = bytes[1];
            px.rgba.b = bytes[2];
        } else if (op == QOI_OP_RGBA) {
            //FIXME: flip
        } else if ((op & QOI_MASK_2) == QOI_OP_INDEX) {
            px = index[op];
        } else if ((op & QOI_MASK_2) == QOI_OP_DIFF) {
            px.rgba.r += ((op >> 4) & 0x03) - 2;
            px.rgba.g += ((op >> 2) & 0x03) - 2;
            px.rgba.b += (op & 0x03) - 2;
        } else if ((op & QOI_MASK_2) == QOI_OP_LUMA) {
            int b2 = bytes[0];
            int vg = (op & 0x3f) - 32;
            px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
            px.rgba.g += vg;
            px.rgba.b += vg - 8 + (b2 & 0x0f);
        } else if ((op & QOI_MASK_2) == QOI_OP_RUN) {
            int run = (op & 0x3f)+1;
//            ESP_LOGD(TAG, "runon %d", run);

            while (run && px_pos < px_len) {

                pixels[px_pos].r = px.rgba.r;
                pixels[px_pos].g = px.rgba.g;
                pixels[px_pos].b = px.rgba.b;
                run--;
                px_pos++;
            }
            wait_for_op = true;
            return (px_pos < px_len);


        } else {
            ESP_LOGE(TAG, "Illegal operation: %d", op);
            return false;
        }

        index[QOI_COLOR_HASH(px) % 64] = px;
//        ESP_LOGD(TAG, "write pixel %d", px_pos);
        pixels[px_pos].r = px.rgba.r;
        pixels[px_pos].g = px.rgba.g;
        pixels[px_pos].b = px.rgba.b;

        px_pos++;
        wait_for_op = true;
        return (px_pos < px_len);

    }


//    void decode(const void *data, int data_size, CRGB pixels[], int px_len) {
//
//        const unsigned char *bytes;
//        unsigned int header_magic;
//        qoi_rgba_t px;
//        int data_pos = 0, run = 0;
//
//        bytes = (const unsigned char *) data;
//
//        px.rgba.r = 0;
//        px.rgba.g = 0;
//        px.rgba.b = 0;
//        px.rgba.a = 255;
//
//        for (int px_pos = 0; px_pos < px_len; px_pos++) {
//            if (run > 0) {
//                run--;
//            } else if (data_pos < data_size) {
//                int b1 = bytes[data_pos++];
//
//                if (b1 == QOI_OP_RGB) {
//                    px.rgba.r = bytes[data_pos++];
//                    px.rgba.g = bytes[data_pos++];
//                    px.rgba.b = bytes[data_pos++];
//                } else if (b1 == QOI_OP_RGBA) {
//                    px.rgba.r = bytes[data_pos++];
//                    px.rgba.g = bytes[data_pos++];
//                    px.rgba.b = bytes[data_pos++];
//                    px.rgba.a = bytes[data_pos++];
//                } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
//                    px = index[b1];
//                } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
//                    px.rgba.r += ((b1 >> 4) & 0x03) - 2;
//                    px.rgba.g += ((b1 >> 2) & 0x03) - 2;
//                    px.rgba.b += (b1 & 0x03) - 2;
//                } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
//                    int b2 = bytes[data_pos++];
//                    int vg = (b1 & 0x3f) - 32;
//                    px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
//                    px.rgba.g += vg;
//                    px.rgba.b += vg - 8 + (b2 & 0x0f);
//                } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
//                    run = (b1 & 0x3f);
//                }
//
//                index[QOI_COLOR_HASH(px) % 64] = px;
//            }
//
//
//            pixels[px_pos].r = px.rgba.r;
//            pixels[px_pos].g = px.rgba.g;
//            pixels[px_pos].b = px.rgba.b;
//
//
//        }
//    }

};


#endif //LEDSTREAM_QOIS_HPP
