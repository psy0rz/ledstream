#ifndef HEADER_LEDS
#define HEADER_LEDS

#include <cstdint>

//universal pixel interface, for multiple underlying librareis (fastled-idf and esp32-hub75 for now)

void leds_init();
void  leds_reset();
void  leds_setNextPixel( uint8_t r,  uint8_t g,  uint8_t b);
void  leds_show();

//QOIS_OP_PREVFRAME support: skip 'count' pixels, keeping their content from the
//previous frame, and return the color of the last kept pixel in r/g/b
//(the decoder needs it to stay in sync with the encoder).
void  leds_keepPixels(uint16_t count, uint8_t* r, uint8_t* g, uint8_t* b);

//blank all pixels (called at the start of a new stream)
void  leds_clear();

extern uint16_t leds_pixels_per_channel;

#endif

