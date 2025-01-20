#include <utils.hpp>

#include "sdkconfig.h"


#ifdef CONFIG_LEDSTREAM_MODE_HUB75

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

//slower, but prevents tearing
#define DOUBLE_BUFFERING 1

MatrixPanel_I2S_DMA* dma_display = nullptr;

uint8_t leds_x = 0;
uint8_t leds_y = 0;

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

#ifdef DOUBLE_BUFFERING
    mxconfig.double_buff = true;
#endif


    dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    dma_display->begin();

    dma_display->setBrightness8(255);
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

    leds_x++;
    if (leds_x >= CONFIG_LEDSTREAM_WIDTH)
    {
        leds_x = 0;
        leds_y++;
        if (leds_y >= CONFIG_LEDSTREAM_HEIGHT)
            leds_y = 0;
    }
}


#endif
