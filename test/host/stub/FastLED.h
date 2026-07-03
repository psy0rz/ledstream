// host-side stub of FastLED.h, just enough to compile qois.hpp
#ifndef FASTLED_STUB_H
#define FASTLED_STUB_H

#include <cstdint>
#include <cstring>
#include <cstdio>

struct CRGB {
    uint8_t r, g, b;
};

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W %s: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...)
#define ESP_LOGI(tag, fmt, ...)

#endif
