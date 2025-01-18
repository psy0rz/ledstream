#ifndef HEADER_LEDS
#define HEADER_LEDS

#include <cstdint>

//universal pixel interface, for multiple underlying librareis (fastled-idf and esp32-hub75 for now)

void leds_init();
void  leds_reset();
void  leds_setNextPixel( uint8_t r,  uint8_t g,  uint8_t b);
void  leds_show();
extern uint16_t leds_pixels_per_channel;

#endif

