// Host-side test of the REAL firmware decoder (main/qois.hpp) and the REAL ws2812 led
// cursor logic (main/leds_ws2812.cpp) against ledder encoder output. Only FastLED and
// the esp glue are stubbed (see stub/).
//
// Feeds a .qois stream byte-by-byte and compares the led buffer against the expected
// raw RGB frames after every frame. Streams and expected frames are produced by the
// ledder repo: node tools/qois/qois-frames.mjs capture && node tools/qois/qois-frames.mjs dump [ppc]
//
// usage: qois_test <stream.qois> <expected.bin> <pixelcount> [loops]
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <FastLED.h> // stub: CRGB + array-registration-tracking FastLED
#include "leds.hpp"  // real universal led interface
#include "qois.hpp"  // real decoder

// the framebuffer defined in leds_ws2812.cpp
extern CRGB leds[CONFIG_LEDSTREAM_CHANNELS][CONFIG_LEDSTREAM_LEDS_PER_CHANNEL];

static std::vector<unsigned char> readAll(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<unsigned char> buf(len);
    if (fread(buf.data(), 1, len, f) != (size_t) len) { perror(path); exit(2); }
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s stream.qois expected.bin pixelcount [loops]\n", argv[0]); return 2; }
    auto stream = readAll(argv[1]);
    auto expected = readAll(argv[2]);
    const int N = atoi(argv[3]);
    const int loops = argc > 4 ? atoi(argv[4]) : 1;
    const int frames = expected.size() / (N * 3);

    if (N > CONFIG_LEDSTREAM_CHANNELS * CONFIG_LEDSTREAM_LEDS_PER_CHANNEL) {
        fprintf(stderr, "pixelcount %d exceeds compiled led buffer\n", N);
        return 2;
    }

    leds_init(); //registers the led arrays with the stub FastLED, so leds_clear() works

    //the led buffer viewed as one flat pixel array, in the order the stream fills it
    const CRGB *flat = &leds[0][0];
    int failures = 0;

    //'loops' simulates the flash replay mode: the same stream restarted from the top,
    //with the full decoder reset Ledstreamer does before every replay
    for (int loop = 0; loop < loops; loop++) {
        qois_resetStream();

        size_t pos = 0;
        int frame = 0;
        while (pos < stream.size()) {
            //feed one byte at a time, like a trickling network stream
            qois_decodeBytes(&stream[pos], 1, 0);
            pos++;

            //frame body complete? (qois only 'shows' it when the next byte arrives)
            if (qois_frame_bytes_left != 0)
                continue;

            //compare led buffer with expected frame
            const unsigned char *want = &expected[(size_t) frame * N * 3];
            for (int i = 0; i < N; i++) {
                if (flat[i].r != want[i * 3] || flat[i].g != want[i * 3 + 1] || flat[i].b != want[i * 3 + 2]) {
                    if (failures < 3)
                        fprintf(stderr, "loop %d frame %d pixel %d: got %d,%d,%d want %d,%d,%d\n",
                                loop, frame, i, flat[i].r, flat[i].g, flat[i].b,
                                want[i * 3], want[i * 3 + 1], want[i * 3 + 2]);
                    failures++;
                    break;
                }
            }
            frame++;
        }

        if (frame != frames) {
            fprintf(stderr, "loop %d: decoded %d of %d frames\n", loop, frame, frames);
            return 2;
        }
    }

    if (failures) {
        printf("FAIL: %d frames mismatched\n", failures);
        return 1;
    }
    printf("OK: %d frames x %d loops decoded exactly\n", frames, loops);
    return 0;
}
