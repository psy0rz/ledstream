// host-side stub of timing.hpp: the test decodes as fast as possible, no frame timing
#pragma once
#include <cstdint>
#include "esp_log.h" // IRAM_ATTR / ESP_LOG* for qois.hpp, which includes us

inline int64_t esp_timer_get_time() { return 0; }
inline void timing_reset(bool buffered = true) { (void) buffered; }
inline void timing_wait_until_us(int64_t) {}
