// host-side stub of leds.hpp: just the persistent led framebuffer
#ifndef LEDS_STUB_HPP
#define LEDS_STUB_HPP

#include "FastLED.h"

CRGB leds[CONFIG_LEDSTREAM_CHANNELS][CONFIG_LEDSTREAM_LEDS_PER_CHANNEL];

#endif
