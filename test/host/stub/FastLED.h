// host-side stub of FastLED: just enough for leds_ws2812.cpp to compile.
// addLeds() registers the arrays so clear() actually blanks them (leds_clear() relies on it).
#pragma once
#include <cstdint>
#include <cstring>

enum EOrder { RGB = 0, GRB = 1 };

struct CRGB {
    uint8_t r, g, b;
};

template <uint8_t DATA_PIN, EOrder ORDER> class WS2811 {};

class CFastLED {
    CRGB* arrays[16] = {};
    int counts[16] = {};
    int n = 0;

public:
    template <template <uint8_t, EOrder> class CHIPSET, uint8_t DATA_PIN, EOrder ORDER>
    void addLeds(CRGB* data, int count) {
        if (n < 16) {
            arrays[n] = data;
            counts[n] = count;
            n++;
        }
    }

    void clear() {
        for (int i = 0; i < n; i++)
            memset(arrays[i], 0, counts[i] * sizeof(CRGB));
    }

    void show() {}
    void setMaxPowerInVoltsAndMilliamps(uint8_t, uint32_t) {}
};

inline CFastLED FastLED;
