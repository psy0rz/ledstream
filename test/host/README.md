# Host-side decoder test

Compiles the real firmware QOIS decoder (`main/qois.hpp`) and the real ws2812 led cursor
logic (`main/leds_ws2812.cpp`) on the host — only FastLED and the esp glue are stubbed
(`stub/`). It feeds a recorded ledder stream byte-by-byte and checks the led buffer
bit-exactly against the raw frames after every frame — including flash-replay style loop
restarts (`qois_resetStream()`) and the persistent-across-frames color index /
`QOIS_OP_PREVFRAME` behavior (see the decoder contract at the top of `main/qois.hpp`).

```sh
make test
```

Runs a 300-frame marquee stream (32x8) in two configurations: 1 channel of 256 leds, and
8 channels of 32 leds (temporal runs crossing channel boundaries), 3 replay loops each.

## Regenerating / adding test vectors

The vectors in `vectors/` come from the [ledder](https://github.com/psy0rz/ledder) repo
(`tools/qois/`): `.bin` is the raw gamma-mapped RGB expected output, `.qois` the encoded
stream. In a compiled ledder checkout:

```sh
node tools/qois/qois-frames.mjs capture    # render animations -> frames/*.bin
node tools/qois/qois-frames.mjs dump       # encode -> frames/*.qois  (header ppc=0)
node tools/qois/qois-frames.mjs dump 32    # encode with pixels-per-channel=32 in the header
```

then copy the `.bin`/`.qois` pair here and add a line to the Makefile. Note that a `.qois`
vector only matches its *own* `.bin` (animations are randomized per capture).
