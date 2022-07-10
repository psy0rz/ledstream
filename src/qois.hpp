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

class Qois {

    qoi_rgba_t index[64];


public:
    Qois() {
        QOI_ZEROARR(index);
    }

    void decode(const void *data, int data_size, CRGB pixels[], int px_len) {

        const unsigned char *bytes;
        unsigned int header_magic;
        qoi_rgba_t px;
        int data_pos = 0, run = 0;

        bytes = (const unsigned char *) data;

        px.rgba.r = 0;
        px.rgba.g = 0;
        px.rgba.b = 0;
        px.rgba.a = 255;

        for (int px_pos = 0; px_pos < px_len; px_pos++) {
            if (run > 0) {
                run--;
            } else if (data_pos < data_size) {
                int b1 = bytes[data_pos++];

                if (b1 == QOI_OP_RGB) {
                    px.rgba.r = bytes[data_pos++];
                    px.rgba.g = bytes[data_pos++];
                    px.rgba.b = bytes[data_pos++];
                } else if (b1 == QOI_OP_RGBA) {
                    px.rgba.r = bytes[data_pos++];
                    px.rgba.g = bytes[data_pos++];
                    px.rgba.b = bytes[data_pos++];
                    px.rgba.a = bytes[data_pos++];
                } else if ((b1 & QOI_MASK_2) == QOI_OP_INDEX) {
                    px = index[b1];
                } else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
                    px.rgba.r += ((b1 >> 4) & 0x03) - 2;
                    px.rgba.g += ((b1 >> 2) & 0x03) - 2;
                    px.rgba.b += (b1 & 0x03) - 2;
                } else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
                    int b2 = bytes[data_pos++];
                    int vg = (b1 & 0x3f) - 32;
                    px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
                    px.rgba.g += vg;
                    px.rgba.b += vg - 8 + (b2 & 0x0f);
                } else if ((b1 & QOI_MASK_2) == QOI_OP_RUN) {
                    run = (b1 & 0x3f);
                }

                index[QOI_COLOR_HASH(px) % 64] = px;
            }


            pixels[px_pos].r = px.rgba.r;
            pixels[px_pos].g = px.rgba.r;
            pixels[px_pos].b = px.rgba.r;


        }
    }

};


#endif //LEDSTREAM_QOIS_HPP
