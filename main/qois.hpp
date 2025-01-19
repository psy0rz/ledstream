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

static int bytes_needed __attribute__((aligned(4)));

static qoi_rgba_t qois_index[64] __attribute__((aligned(4)));
static qoi_rgba_t qois_px __attribute__((aligned(4)));
static unsigned char qois_bytes[6] __attribute__((aligned(4)));
static int qois_bytes_index __attribute__((aligned(4)));
static int qois_op __attribute__((aligned(4)));
static bool qois_wait_for_op __attribute__((aligned(4)));

//    int px_len;
static bool qois_wait_for_header __attribute__((aligned(4)));

//    CRGB *pixels;
static uint16_t qois_show_time __attribute__((aligned(4)));

int64_t qois_local_time_offset=0;
int64_t qois_local_show_time=0;


static uint16_t qois_frame_bytes_left __attribute__((aligned(4)));


//get ready for next frame
inline void IRAM_ATTR qois_reset()
{
    //        ESP_LOGD(UDPBUFFER_TAG, "start frame");

    qois_px.rgba.r = 0;
    qois_px.rgba.g = 0;
    qois_px.rgba.b = 0;
    qois_px.rgba.a = 1;


    //stuff added to make QOI byte based
    qois_wait_for_op = false;
    qois_wait_for_header = true;
    bytes_needed = 6; //frame length + pixels per channel + display time
    qois_bytes_index = 0;
    qois_frame_bytes_left = 6;

    //todo: keep
    QOI_ZEROARR(qois_index);

    leds_reset();
}

//sets the next pixel, keeping in mind pixel_per_channel (what ledder says we SHOULD use)
// and CONFIG_LEDSTREAM_LEDS_PER_CHANNEL (whats actually statically compiled in ledstream)

//returns true when we need more data. false means frame is complete, ready to show at show_time.
inline  IRAM_ATTR bool qois_decodeBytes(const uint8_t buffer[], uint16_t buffer_len, uint16_t& buffer_offset)
{
    uint8_t data;
    while (buffer_offset < buffer_len)
    {
        //        ESP_LOGD(UDPBUFFER_TAG, "decode byte: data=%d waitshowtime=%d, waitop=%d, bytes_needed=%d, op=%d", data,wait_for_header, wait_for_op, bytes_needed, op);
        if (qois_frame_bytes_left == 0)
        {
            return false;
        }

        //consume next byte from input buffer
        data = buffer[buffer_offset];
        buffer_offset += 1;
        qois_frame_bytes_left--;

        //step 1: store received bytes, if we need them
        if (bytes_needed)
        {
            bytes_needed--;
            qois_bytes[qois_bytes_index] = data;
            qois_bytes_index++;

            //still need more?
            if (bytes_needed)
                continue;
        }

        //step 2: parse frameheader (from stored bytes in step 1)
        if (qois_wait_for_header)
        {
            qois_wait_for_header = false;
            qois_wait_for_op = true;

            //bytes 0-1, total size of this frame:
            qois_frame_bytes_left = *(uint16_t*)&qois_bytes[0];
            qois_frame_bytes_left = qois_frame_bytes_left - 6; //we already used 6 for this header

            //bytes 2-3:
            //pixels per channel
            leds_pixels_per_channel = *(uint16_t*)&qois_bytes[2];


            //bytes 4-5:
            qois_show_time = *(uint16_t*)&qois_bytes[4];

            qois_local_show_time=qois_local_time_offset+(qois_show_time*1000);
            if (abs(qois_local_show_time-esp_timer_get_time())>1000000)
            {
                ESP_LOGI(QOISTAG,"Resettting local time offset");
                qois_local_time_offset = esp_timer_get_time()-(qois_show_time*1000);
                qois_local_show_time=qois_local_time_offset+(qois_show_time*1000);
            }else if (qois_local_show_time-esp_timer_get_time()<0)
            {
                //we're behind, correct a bit
                ESP_LOGI(QOISTAG,"Correcting time");
                qois_local_time_offset=qois_local_time_offset+1000;

            }

            //            ESP_LOGD(UDPBUFFER_TAG, "got header: showtime=%u, frame_length=%u", show_time, frame_bytes_left);
            continue;
        }

        //step3: store actual qois operation, and get more bytes if the operation needs it
        if (qois_wait_for_op)
        {
            qois_op = data;
            qois_bytes_index = 0;
            qois_wait_for_op = false;

            if (qois_op == QOI_OP_RGB)
                bytes_needed = 3;
            else if ((qois_op & QOI_MASK_2) == QOI_OP_LUMA)
                bytes_needed = 1;
            else
                bytes_needed = 0;

            if (bytes_needed)
                continue;
        }


        //step4: from this point on we know the operation and we have all the bytes we need to execute the QOI operation
        //now we loop between step 3 and 4 until the frame is complete.

        if (qois_op == QOI_OP_RGB)
        {
            //            ESP_LOGD(UDPBUFFER_TAG, "RGB");

            qois_px.rgba.r = qois_bytes[0];
            qois_px.rgba.g = qois_bytes[1];
            qois_px.rgba.b = qois_bytes[2];
        }
        else if (qois_op == QOI_OP_RGBA)
        {
            //            ESP_LOGD(UDPBUFFER_TAG, "RGBA ??");
            //FIXME: flip
        }
        else if ((qois_op & QOI_MASK_2) == QOI_OP_INDEX)
        {
            //            ESP_LOGD(UDPBUFFER_TAG, "index");
            qois_px = qois_index[qois_op];
        }
        else if ((qois_op & QOI_MASK_2) == QOI_OP_DIFF)
        {
            //            ESP_LOGD(UDPBUFFER_TAG, "diff");
            qois_px.rgba.r += ((qois_op >> 4) & 0x03) - 2;
            qois_px.rgba.g += ((qois_op >> 2) & 0x03) - 2;
            qois_px.rgba.b += (qois_op & 0x03) - 2;
        }
        else if ((qois_op & QOI_MASK_2) == QOI_OP_LUMA)
        {
            //            ESP_LOGD(UDPBUFFER_TAG, "luma");
            int b2 = qois_bytes[0];
            int vg = (qois_op & 0x3f) - 32;
            qois_px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
            qois_px.rgba.g += vg;
            qois_px.rgba.b += vg - 8 + (b2 & 0x0f);
        }
        else if ((qois_op & QOI_MASK_2) == QOI_OP_RUN)
        {
            int run = (qois_op & 0x3f) + 1;

            while (run)
            {
                 leds_setNextPixel(qois_px.rgba.r, qois_px.rgba.g, qois_px.rgba.b);
                run--;
            }
            qois_wait_for_op = true;
            continue;
        }
        else
        {
            ESP_LOGE(QOISTAG, "Illegal operation: %d", qois_op);
            continue;
        }

        qois_px.rgba.a = 255;
        qois_index[QOI_COLOR_HASH(qois_px) % 64] = qois_px;
        //        ESP_LOGD(UDPBUFFER_TAG, "write pixel %d", px_pos);

        leds_setNextPixel(qois_px.rgba.r, qois_px.rgba.g, qois_px.rgba.b);
        qois_wait_for_op = true;
    }

    return true;

}


#endif //LEDSTREAM_QOIS_HPP
