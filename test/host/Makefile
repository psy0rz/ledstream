CXX ?= g++
# -Wno-unused-variable: qois_show() keeps variables for its commented-out timing code
CXXFLAGS = -std=c++17 -O2 -Wall -Wno-unused-variable -Istub -I../../main

SRC = qois_test.cpp ../../main/leds_ws2812.cpp
DEPS = $(SRC) ../../main/qois.hpp ../../main/leds.hpp stub/FastLED.h stub/esp_log.h stub/sdkconfig.h stub/timing.hpp

VEC = vectors/Text_Marquee-github-32x8

# 1ch: header pixels-per-channel is 0, decoder falls back to the compiled-in value.
# 8ch: header says 32 pixels per channel, exercises PREVFRAME runs crossing channels.
test: qois_test_1ch qois_test_8ch
	./qois_test_1ch $(VEC).qois $(VEC).bin 256 3
	./qois_test_8ch $(VEC)-32ppc.qois $(VEC).bin 256 3

qois_test_1ch: $(DEPS)
	$(CXX) $(CXXFLAGS) -DCONFIG_LEDSTREAM_MODE_WS2812 -DCONFIG_LEDSTREAM_CHANNELS=1 -DCONFIG_LEDSTREAM_LEDS_PER_CHANNEL=256 -o $@ $(SRC)

qois_test_8ch: $(DEPS)
	$(CXX) $(CXXFLAGS) -DCONFIG_LEDSTREAM_MODE_WS2812 -DCONFIG_LEDSTREAM_CHANNELS=8 -DCONFIG_LEDSTREAM_LEDS_PER_CHANNEL=32 -o $@ $(SRC)

clean:
	rm -f qois_test_1ch qois_test_8ch

.PHONY: test clean
