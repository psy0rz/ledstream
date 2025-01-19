//
// Created by psy on 7/10/22.
//

//QOIS decoder:
//-You keep feeding it bytes (decodeByte())
//-It will call leds_setNextPixel for every decoded pixel.
//-When its done it will return false and the show_time will be set to the absolute time in mS when to show the frame.
//-Its up to the caller to call leds_show() at precisely the show_time.
//-Call reset to get it ready for the next frame. (this will also call leds_reset())

//NOTE: no longer a class, so we can optimize for speed with IRAM_ATTR


#ifndef LEDSTREAM_QOIS_HPP
#define LEDSTREAM_QOIS_HPP
#include <cstring>

#include <leds.hpp>

#define QOI_ZEROARR(a) memset((a),0,sizeof(a))

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOI_OP_RGBA   0xff /* 11111111 */

#define QOI_MASK_2    0xc0 /* 11000000 */
#define QOI_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)

static const char* QOISTAG = "qois";


typedef union
{
    struct
    {
        unsigned char r, g, b, a;
    } rgba;

    unsigned int v;
} qoi_rgba_t;

static IRAM_ATTR int bytes_needed;

qoi_rgba_t qois_index[64];
qoi_rgba_t px;
unsigned char bytes[6];
int qois_bytes_received;
int qois_op;
bool qois_wait_for_op;

//    int px_len;
bool qois_wait_for_header;

//    CRGB *pixels;
uint16_t qois_show_time;
uint16_t qois_frame_bytes_left;


//get ready for next frame
inline void qois_reset()
{
    //        ESP_LOGD(UDPBUFFER_TAG, "start frame");

    px.rgba.r = 0;
    px.rgba.g = 0;
    px.rgba.b = 0;
    px.rgba.a = 1;


    //stuff added to make QOI byte based
    qois_wait_for_op = false;
    qois_wait_for_header = true;
    bytes_needed = 6; //frame length + pixels per channel + display time
    qois_bytes_received = 0;
    qois_frame_bytes_left = 6;

    //todo: keep
    QOI_ZEROARR(qois_index);

    leds_reset();
}

//sets the next pixel, keeping in mind pixel_per_channel (what ledder says we SHOULD use)
// and CONFIG_LEDSTREAM_LEDS_PER_CHANNEL (whats actually statically compiled in ledstream)

//returns true when we need more data. false means frame is complete
inline bool IRAM_ATTR qois_decodeByte(uint8_t data)
{
    //        ESP_LOGD(UDPBUFFER_TAG, "decode byte: data=%d waitshowtime=%d, waitop=%d, bytes_needed=%d, op=%d", data,wait_for_header, wait_for_op, bytes_needed, op);
    if (qois_frame_bytes_left == 0)
    {
        return false;
    }
    qois_frame_bytes_left--;

    //step 1: store received bytes, if we need them
    if (bytes_needed)
    {
        bytes_needed--;
        bytes[qois_bytes_received] = data;
        qois_bytes_received++;

        //still need more?
        if (bytes_needed)
            return (qois_frame_bytes_left > 0);
    }

    //step 2: parse frameheader (from stored bytes in step 1)
    if (qois_wait_for_header)
    {
        qois_wait_for_header = false;
        qois_wait_for_op = true;

        //bytes 0-1, total size of this frame:
        qois_frame_bytes_left = *(uint16_t*)&bytes[0];
        qois_frame_bytes_left = qois_frame_bytes_left - 6; //we already used 6 for this header

        //bytes 2-3:
        //pixels per channel
        leds_pixels_per_channel = *(uint16_t*)&bytes[2];


        //bytes 4-5:
        qois_show_time = *(uint16_t*)&bytes[4];

        //            ESP_LOGD(UDPBUFFER_TAG, "got header: showtime=%u, frame_length=%u", show_time, frame_bytes_left);
        return (qois_frame_bytes_left > 0);
    }

    //step3: store actual qois operation, and get more bytes if the operation needs it
    if (qois_wait_for_op)
    {
        qois_op = data;
        qois_bytes_received = 0;
        qois_wait_for_op = false;

        if (qois_op == QOI_OP_RGB)
            bytes_needed = 3;
        else if ((qois_op & QOI_MASK_2) == QOI_OP_LUMA)
            bytes_needed = 1;
        else
            bytes_needed = 0;

        if (bytes_needed)
            return (qois_frame_bytes_left > 0);
    }


    //step4: from this point on we know the operation and we have all the bytes we need to execute the QOI operation
    //now we loop between step 3 and 4 until the frame is complete.

    if (qois_op == QOI_OP_RGB)
    {
        //            ESP_LOGD(UDPBUFFER_TAG, "RGB");

        px.rgba.r = bytes[0];
        px.rgba.g = bytes[1];
        px.rgba.b = bytes[2];
    }
    else if (qois_op == QOI_OP_RGBA)
    {
        //            ESP_LOGD(UDPBUFFER_TAG, "RGBA ??");
        //FIXME: flip
    }
    else if ((qois_op & QOI_MASK_2) == QOI_OP_INDEX)
    {
        //            ESP_LOGD(UDPBUFFER_TAG, "index");
        px = qois_index[qois_op];
    }
    else if ((qois_op & QOI_MASK_2) == QOI_OP_DIFF)
    {
        //            ESP_LOGD(UDPBUFFER_TAG, "diff");
        px.rgba.r += ((qois_op >> 4) & 0x03) - 2;
        px.rgba.g += ((qois_op >> 2) & 0x03) - 2;
        px.rgba.b += (qois_op & 0x03) - 2;
    }
    else if ((qois_op & QOI_MASK_2) == QOI_OP_LUMA)
    {
        //            ESP_LOGD(UDPBUFFER_TAG, "luma");
        int b2 = bytes[0];
        int vg = (qois_op & 0x3f) - 32;
        px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
        px.rgba.g += vg;
        px.rgba.b += vg - 8 + (b2 & 0x0f);
    }
    else if ((qois_op & QOI_MASK_2) == QOI_OP_RUN)
    {
        int run = (qois_op & 0x3f) + 1;

        while (run)
        {
            leds_setNextPixel(px.rgba.r, px.rgba.g, px.rgba.b);
            run--;
        }
        qois_wait_for_op = true;
        return (qois_frame_bytes_left > 0);
    }
    else
    {
        ESP_LOGE(QOISTAG, "Illegal operation: %d", qois_op);
        return (qois_frame_bytes_left > 0);
    }

    px.rgba.a = 255;
    qois_index[QOI_COLOR_HASH(px) % 64] = px;
    //        ESP_LOGD(UDPBUFFER_TAG, "write pixel %d", px_pos);

    leds_setNextPixel(px.rgba.r, px.rgba.g, px.rgba.b);
    qois_wait_for_op = true;
    return (qois_frame_bytes_left > 0);
}


#endif //LEDSTREAM_QOIS_HPP
