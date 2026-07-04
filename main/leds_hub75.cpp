#include <utils.hpp>

#include <cstring>

#include "sdkconfig.h"

#ifdef CONFIG_LEDSTREAM_MODE_HUB75

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

//prevents tearing, but TOO slow. (animations like HSNL will skip frames)
//also jittery
//NOTE: incompatible with QOIS_OP_PREVFRAME: the back buffer holds frame N-2, not N-1.
// #define DOUBLE_BUFFERING 1

MatrixPanel_I2S_DMA* dma_display = nullptr;

uint8_t leds_x = 0;
uint8_t leds_y = 0;

//the DMA library can't read pixels back, but QOIS_OP_PREVFRAME needs the color of the
//last kept pixel, so keep a shadow copy of what was drawn
static uint8_t leds_shadow[CONFIG_LEDSTREAM_HEIGHT][CONFIG_LEDSTREAM_WIDTH][3];

//unused in this library for now
uint16_t leds_pixels_per_channel = 0;

void leds_init()
{
    ESP_LOGI("leds", "Initializing HUB75 library...");

    HUB75_I2S_CFG::i2s_pins _pins = {
        CONFIG_LEDSTREAM_R1,
        CONFIG_LEDSTREAM_G1,
        CONFIG_LEDSTREAM_B1,
        CONFIG_LEDSTREAM_R2,
        CONFIG_LEDSTREAM_G2,
        CONFIG_LEDSTREAM_B2,
        CONFIG_LEDSTREAM_A,
        CONFIG_LEDSTREAM_B,
        CONFIG_LEDSTREAM_C,
        CONFIG_LEDSTREAM_D,
        CONFIG_LEDSTREAM_E,
        CONFIG_LEDSTREAM_LAT,
        CONFIG_LEDSTREAM_OE,
        CONFIG_LEDSTREAM_CLK,

    };
    HUB75_I2S_CFG mxconfig(CONFIG_LEDSTREAM_WIDTH, CONFIG_LEDSTREAM_HEIGHT, CONFIG_LEDSTREAM_CHAIN, _pins);


    mxconfig.clkphase=false;

    //panel updates are written into the live DMA buffer unsynchronized to the panel
    //scan, so a new frame becomes visible at the next scan pass: a quantization of up
    //to one scan period. At the default ~110Hz that is 9mS, beating against 60fps
    //content as very visible 10Hz judder. A higher scan rate shrinks the window and
    //pushes the beat above the visible range, at the cost of some color depth.
    mxconfig.min_refresh_rate = 240;
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_16M;

#ifdef DOUBLE_BUFFERING
    mxconfig.double_buff = true;
#endif


    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();

    dma_display->setBrightness8(255);

    //DEBUG: a scan rate close to the 60fps stream rate causes beat-frequency judder,
    //since we write into the live DMA buffer unsynchronized to the panel scan
    ESP_LOGI("leds", "HUB75 panel scan rate: %d Hz", dma_display->calculated_refresh_rate);
}


void IRAM_ATTR leds_reset()
{
    leds_x = 0;
    leds_y = 0;
}

void IRAM_ATTR leds_show()
{
#ifdef DOUBLE_BUFFERING
    dma_display->flipDMABuffer();
#endif
}


void IRAM_ATTR leds_setNextPixel(const uint8_t r, const uint8_t g, const uint8_t b)
{
    dma_display->drawPixelRGB888(leds_x, leds_y, r, g, b);
    leds_shadow[leds_y][leds_x][0] = r;
    leds_shadow[leds_y][leds_x][1] = g;
    leds_shadow[leds_y][leds_x][2] = b;

    leds_x++;
    if (leds_x >= CONFIG_LEDSTREAM_WIDTH)
    {
        leds_x = 0;
        leds_y++;
        if (leds_y >= CONFIG_LEDSTREAM_HEIGHT)
            leds_y = 0;
    }
}


void IRAM_ATTR leds_keepPixels(const uint16_t count, uint8_t* r, uint8_t* g, uint8_t* b)
{
    if (count == 0)
        return;

    //skip over the kept pixels: the display (single-buffered) still shows them
    const uint32_t total = CONFIG_LEDSTREAM_WIDTH * CONFIG_LEDSTREAM_HEIGHT;
    const uint32_t last = ((uint32_t)leds_y * CONFIG_LEDSTREAM_WIDTH + leds_x + count - 1) % total;
    const uint32_t next = (last + 1) % total;
    leds_x = next % CONFIG_LEDSTREAM_WIDTH;
    leds_y = next / CONFIG_LEDSTREAM_WIDTH;

    const uint32_t last_x = last % CONFIG_LEDSTREAM_WIDTH;
    const uint32_t last_y = last / CONFIG_LEDSTREAM_WIDTH;
    *r = leds_shadow[last_y][last_x][0];
    *g = leds_shadow[last_y][last_x][1];
    *b = leds_shadow[last_y][last_x][2];
}


void leds_clear()
{
    if (dma_display)
        dma_display->clearScreen();
    memset(leds_shadow, 0, sizeof(leds_shadow));
}


#endif
