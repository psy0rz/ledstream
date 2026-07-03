// Host-side test of the REAL firmware decoder (main/qois.hpp) against ledder encoder output.
// Feeds a .qois stream byte-by-byte exactly like Ledstreamer::process() does and
// compares the led buffer against the expected raw RGB frames after every frame.
//
// Streams and expected frames are produced by the ledder repo:
//   node tools/qois/qois-frames.mjs capture && node tools/qois/qois-frames.mjs dump [ppc]
//
// usage: qois_test <stream.qois> <expected.bin> <pixelcount> [loops]
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "qois.hpp"

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

    //the led buffer viewed as one flat pixel array, in the order the stream fills it
    const CRGB *flat = &leds[0][0];

    Qois qois;
    int failures = 0;

    //'loops' simulates the flash replay mode: the same stream restarted from the top,
    //with a full decoder reset like Ledstreamer does at fileserver_read_offset==0
    for (int loop = 0; loop < loops; loop++) {
        qois.resetStream();
        memset(leds, 0, sizeof(leds)); //FastLED.clear()

        size_t pos = 0;
        for (int f = 0; f < frames; f++) {
            //feed bytes until the decoder reports the frame is complete
            while (qois.decodeByte(stream[pos++])) {
                if (pos > stream.size()) { fprintf(stderr, "stream ran out at frame %d\n", f); return 2; }
            }

            //compare led buffer with expected frame
            const unsigned char *want = &expected[(size_t) f * N * 3];
            for (int i = 0; i < N; i++) {
                if (flat[i].r != want[i * 3] || flat[i].g != want[i * 3 + 1] || flat[i].b != want[i * 3 + 2]) {
                    if (failures < 3)
                        fprintf(stderr, "loop %d frame %d pixel %d: got %d,%d,%d want %d,%d,%d\n",
                                loop, f, i, flat[i].r, flat[i].g, flat[i].b,
                                want[i * 3], want[i * 3 + 1], want[i * 3 + 2]);
                    failures++;
                    break;
                }
            }

            qois.nextFrame(); //like Ledstreamer after FastLED.show()
        }
        if (pos != stream.size()) {
            fprintf(stderr, "loop %d: consumed %zu of %zu stream bytes\n", loop, pos, stream.size());
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
