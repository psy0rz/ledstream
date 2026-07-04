//
// Created by psy on 7/10/22.
//

//QOIS decoder:
//-You keep feeding it bytes (decodeByte())
//-It will call leds_setNextPixel for every decoded pixel.
//-When its done it will return false and the show_time will be set to the absolute time in mS when to show the frame.
//-Its up to the caller to call leds_show() at precisely the show_time.
//-Call reset to get it ready for the next frame. (this will also call leds_reset())

/* DECODER CONTRACT (differs from stock QOI, must match ledder's DisplayQOIS encoder):
 *  - the led framebuffer and the 64-color index persist across frames within a stream;
 *    both start zeroed/black at the start of a stream (qois_resetStream()). The ledder
 *    encoder resets its mirror of this state per connection, so call qois_resetStream()
 *    whenever a stream starts from its beginning (new http connection, flash replay loop).
 *  - the previous-pixel state resets to black at the start of every frame.
 *  - the color-index is updated ONLY by DIFF/LUMA/RGB ops. RUN, INDEX and PREVFRAME ops
 *    must NOT touch the index -- stock QOI updates it after every op, which desyncs from
 *    the encoder now that the index persists across frames.
 *  - QOIS_OP_PREVFRAME (reuses the never-emitted QOI_OP_RGBA byte 0xff): keep the next N
 *    pixels from the previous frame (2 bytes little-endian N follow); the previous-pixel
 *    state becomes the last kept pixel.
 */

//NOTE: no longer a class, so we can optimize for speed with IRAM_ATTR


#ifndef LEDSTREAM_QOIS_HPP
#define LEDSTREAM_QOIS_HPP
#include <cstring>

#include <leds.hpp>
#include <timing.hpp>

#define QOI_ZEROARR(a) memset((a),0,sizeof(a))

#define QOI_OP_INDEX  0x00 /* 00xxxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_RUN    0xc0 /* 11xxxxxx */
#define QOI_OP_RGB    0xfe /* 11111110 */
#define QOIS_OP_PREVFRAME 0xff /* 11111111, was QOI_OP_RGBA which is never emitted */

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
static int64_t qois_show_time_us __attribute__((aligned(4))) = 0;
static int64_t qois_time_offset_us __attribute__((aligned(4))) = 0;

// int64_t qois_local_time_offset = 0;
// int64_t qois_local_show_time = 0;


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

    //NOTE: the color index is NOT cleared here: it persists across frames
    //(see decoder contract above)

    leds_reset();
}

//get ready for a new stream: clears the state that persists across frames.
//Call at the start of every stream (new http connection, flash replay restart),
//so decoder and encoder start from the same empty state.
inline void qois_resetStream()
{
    QOI_ZEROARR(qois_index);
    leds_clear();
    qois_reset();
}


#define QOIS_TIME_STEP_US 100
#define QOIS_TIME_MAX_DIFF_US 500000

//does timing and displaying of actual descoded buffer
inline void IRAM_ATTR qois_show()
{
    int64_t now = esp_timer_get_time(); //100
    int64_t local_show_time = qois_time_offset_us +qois_show_time_us; //91+10=101

    // int64_t diff = local_show_time - now;//101-100=1
    //
    //
    // if (abs(diff) > QOIS_TIME_MAX_DIFF_US)
    // {
    //     //difference too big, step correction
    //     ESP_LOGI(QOISTAG, "Resetting local time offset");
    //     qois_time_offset_us = now-qois_show_time_us+16000; //start by lagging 1 frame (16ms)
    //     local_show_time = now;
    // }
    // else
    // {
    //     //we never want to be too late, so keep increasing the offset until this is the case.
    //     if (diff < 0)
    //     {
    //         ESP_LOGI(QOISTAG, "%lld mS late", -diff/1000);
    //         qois_time_offset_us = qois_time_offset_us + QOIS_TIME_STEP_US;
    //     }
    // }

    //use task delay:
    // TickType_t xLastWakeTime;
    // xLastWakeTime = xTaskGetTickCount();
    //
    // int32_t wait=(local_show_time-esp_timer_get_time())/1000/portTICK_PERIOD_MS;
    // if (wait>0)
    // {
    //     // ESP_LOGI(QOISTAG, "%d tick", wait);
    //     vTaskDelayUntil( &xLastWakeTime, wait );
    // }

    //busyloop delay
    // ESP_LOGI(QOISTAG, "%lld ms", (local_show_time-esp_timer_get_time())/1000);
    // while (esp_timer_get_time()<local_show_time)
    // {
    //
    // }

    //esp timer delay
    timing_wait_until_us(qois_show_time_us);

    leds_show();
    qois_reset();
}


//processes and displays the whole buffer
inline IRAM_ATTR void qois_decodeBytes(const uint8_t buffer[], uint16_t buffer_len, uint16_t buffer_offset)
{
    uint8_t data;
    while (buffer_offset < buffer_len)
    {
        //frame complete? show
        if (qois_frame_bytes_left == 0)
        {
            qois_show();
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

            //re-reset the leds now that pixels_per_channel is known: leds_reset()
            //replaces a 0 value with the compiled-in default, and the cursor math in
            //leds_keepPixels() needs a valid pixels_per_channel from the very first frame
            leds_reset();

            //bytes 4-5:
            qois_show_time_us = (*(uint16_t*)&qois_bytes[4]) * 1000;


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
            else if (qois_op == QOIS_OP_PREVFRAME)
                bytes_needed = 2;
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
        else if (qois_op == QOIS_OP_PREVFRAME)
        {
            //keep the next N pixels from the previous frame; the previous-pixel state
            //becomes the last kept pixel. Does NOT update the color index.
            uint16_t keep = *(uint16_t*)&qois_bytes[0];
            leds_keepPixels(keep, &qois_px.rgba.r, &qois_px.rgba.g, &qois_px.rgba.b);
            qois_wait_for_op = true;
            continue;
        }
        else if ((qois_op & QOI_MASK_2) == QOI_OP_INDEX)
        {
            //            ESP_LOGD(UDPBUFFER_TAG, "index");
            qois_px = qois_index[qois_op];

            //INDEX must not update the color index (see decoder contract)
            leds_setNextPixel(qois_px.rgba.r, qois_px.rgba.g, qois_px.rgba.b);
            qois_wait_for_op = true;
            continue;
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
}


#endif //LEDSTREAM_QOIS_HPP
