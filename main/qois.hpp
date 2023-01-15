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
    bool wait_for_op;

    CRGB *pixels;
    int px_len;
    bool wait_for_header;

public:
    uint16_t show_time;
    uint16_t frame_bytes_left;


    Qois(CRGB *pixels, int px_len) {
        this->px_len = px_len;
        this->pixels = pixels;
        QOI_ZEROARR(index);
        nextFrame();
    }

    //get ready for next frame
    void nextFrame() {
//        ESP_LOGD(TAG, "start frame");

        px.rgba.r = 0;
        px.rgba.g = 0;
        px.rgba.b = 0;
        px.rgba.a = 1;
        px_pos = 0;

        //stuff added to make QOI byte based
        wait_for_op = false;
        wait_for_header = true;
        bytes_needed = 4;
        bytes_received = 0;
        frame_bytes_left = 4;

//todo: keep
        QOI_ZEROARR(index);

    }

    //returns true when we need more data. false means frame is complete
    bool decodeByte(uint8_t data) {

//        ESP_LOGD(TAG, "decode byte: data=%d waitshowtime=%d, waitop=%d, bytes_needed=%d, op=%d", data,wait_for_header, wait_for_op, bytes_needed, op);
//ESP_LOGD(TAG, "framebytes=%d", frame_bytes_left);
        if (frame_bytes_left == 0) {

            return false;
        }
        frame_bytes_left--;

//        if (!wait_for_header)
//            return true;

//too many pixels, just drop the data
        if (px_pos >= px_len) {
            ESP_LOGE(TAG, "too many pixels (pos=%d len=%d)", px_pos, px_len);
            return true;
        }


        //wait for next operation
        if (wait_for_op) {
            op = data;
            bytes_received = 0;
            wait_for_op = false;

            if (op == QOI_OP_RGB)
                bytes_needed = 3;
            else if ((op & QOI_MASK_2) == QOI_OP_LUMA)
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

        //store showtime and framebytes?
        if (wait_for_header) {
            wait_for_header = false;
            wait_for_op = true;
            frame_bytes_left = *(uint16_t *) &bytes[0];
            show_time = *(uint16_t *) &bytes[2];


//            ESP_LOGD(TAG, "got header: showtime=%u, frame_length=%u", show_time, frame_bytes_left);
            frame_bytes_left = frame_bytes_left - 4; //we already used 4 for this header
//            Serial.println(show_time, HEX);
            return true;
        }



        //from this point on we know the operation and we have all the bytes we need for QOI

        if (op == QOI_OP_RGB) {
//            ESP_LOGD(TAG, "RGB");

            px.rgba.r = bytes[0];
            px.rgba.g = bytes[1];
            px.rgba.b = bytes[2];
        } else if (op == QOI_OP_RGBA) {
//            ESP_LOGD(TAG, "RGBA ??");
            //FIXME: flip
        } else if ((op & QOI_MASK_2) == QOI_OP_INDEX) {
//            ESP_LOGD(TAG, "index");
            px = index[op];
        } else if ((op & QOI_MASK_2) == QOI_OP_DIFF) {
            //            ESP_LOGD(TAG, "diff");
            px.rgba.r += ((op >> 4) & 0x03) - 2;
            px.rgba.g += ((op >> 2) & 0x03) - 2;
            px.rgba.b += (op & 0x03) - 2;
        } else if ((op & QOI_MASK_2) == QOI_OP_LUMA) {
            //            ESP_LOGD(TAG, "luma");
            int b2 = bytes[0];
            int vg = (op & 0x3f) - 32;
            px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
            px.rgba.g += vg;
            px.rgba.b += vg - 8 + (b2 & 0x0f);
        } else if ((op & QOI_MASK_2) == QOI_OP_RUN) {
            int run = (op & 0x3f) + 1;

            while (run && px_pos < px_len) {

                pixels[px_pos].r = px.rgba.r;
                pixels[px_pos].g = px.rgba.g;
                pixels[px_pos].b = px.rgba.b;
                run--;
                px_pos++;
            }
            wait_for_op = true;
            return true;

        } else {
            ESP_LOGE(TAG, "Illegal operation: %d", op);
            return true;
        }

        px.rgba.a = 255;
        index[QOI_COLOR_HASH(px) % 64] = px;
        //        ESP_LOGD(TAG, "write pixel %d", px_pos);
        pixels[px_pos].r = px.rgba.r;
        pixels[px_pos].g = px.rgba.g;
        pixels[px_pos].b = px.rgba.b;

        px_pos++;
        wait_for_op = true;
        return true;

    }


};


#endif //LEDSTREAM_QOIS_HPP
