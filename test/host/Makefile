CXX ?= g++
CXXFLAGS = -std=c++17 -O2 -Wall -Istub -I../../main

VEC = vectors/Text_Marquee-github-32x8

# 1ch: header pixels-per-channel is 0, decoder falls back to the compiled-in value.
# 8ch: header says 32 pixels per channel, exercises PREVFRAME runs crossing channels.
test: qois_test_1ch qois_test_8ch
	./qois_test_1ch $(VEC).qois $(VEC).bin 256 3
	./qois_test_8ch $(VEC)-32ppc.qois $(VEC).bin 256 3

qois_test_1ch: qois_test.cpp ../../main/qois.hpp stub/FastLED.h stub/leds.hpp
	$(CXX) $(CXXFLAGS) -DCONFIG_LEDSTREAM_CHANNELS=1 -DCONFIG_LEDSTREAM_LEDS_PER_CHANNEL=256 -o $@ qois_test.cpp

qois_test_8ch: qois_test.cpp ../../main/qois.hpp stub/FastLED.h stub/leds.hpp
	$(CXX) $(CXXFLAGS) -DCONFIG_LEDSTREAM_CHANNELS=8 -DCONFIG_LEDSTREAM_LEDS_PER_CHANNEL=32 -o $@ qois_test.cpp

clean:
	rm -f qois_test_1ch qois_test_8ch

.PHONY: test clean
