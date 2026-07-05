# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Ledstream is ESP32 firmware that receives compressed led animation streams ("QOIS") from a [ledder](https://github.com/psy0rz/ledder) server and drives either WS2812-style led strings (up to 8 parallel channels via FastLED) or HUB75 RGB matrix panels (via DMA). It can also record a stream to flash and replay it in a loop when the server is unreachable.

**Ledder and this firmware speak a custom stream dialect and must be deployed together** — the QOIS format is versioned in lockstep with `ledder/server/DisplayQOIS.ts`, and old/new combinations desync in both directions.

## Build

ESP-IDF **v4.4.8 exactly** — version 5 does not build yet (see "Upgrading ESP-IDF" below). Easiest via docker:

```sh
git submodule update --init                                  # HUB75 library
./dockerrun idf.py -D SDKCONFIG=somename menuconfig          # config lives under "LEDSTREAM CONFIG"
./dockerrun idf.py -D SDKCONFIG=somename build
./dockerrun idf.py -D SDKCONFIG=somename partition-table-flash
./dockerrun idf.py -D SDKCONFIG=somename flash monitor
```

- `dockerrun` wraps `espressif/idf:v4.4.8`. Multiple board configs are kept as separate `sdkconfig.<name>` files via `-D SDKCONFIG=`.
- **ESP32-S3 + HUB75** builds use `./dockerrun ./esps3idf ...` instead of `idf.py` (sets the esp32s3 target, its own build dir, and restricts COMPONENTS to the HUB75 library).
- `./autocom` is a picocom serial-monitor loop that pauses while esptool is flashing; `mon` similar.
- Partition table: A/B OTA app slots (1M each) plus a 1900K `ledstream` littlefs partition holding the recorded stream (`/littlefs/stream.bin`).
- Key Kconfig options (`main/Kconfig.projbuild`, all `CONFIG_LEDSTREAM_*`): wifi credentials, `LEDDER_URL`, `FIRMWARE_UPGRADE_URL` (OTA), output mode choice `MODE_WS2812`/`MODE_HUB75`, ws2812 `CHANNELS`/`LEDS_PER_CHANNEL`/`CHANNEL<n>_PIN`/`MAX_MILLIAMPS`, HUB75 `WIDTH`/`HEIGHT`/`CHAIN` + pin mapping, optional internal ethernet (LAN87xx).

There is no CI. The only automated test is the host-side decoder test (see below).

## Architecture

Almost everything is header-only `.hpp` in `main/` (inline functions, file-scope `static` state, `IRAM_ATTR` on hot paths — a deliberate speed optimization, e.g. `qois.hpp` says "no longer a class, so we can optimize for speed").

`app_main` (`main/main.cpp`): NVS → `settings_init` → netif/event loop → `console_init` → wifi → `fileserver_init` → `leds_init` (+ optional ethernet) → `timing_init` → starts the http-streamer and flash-replay tasks. (`led_test()`/`timing_test()` are commented-in-place hardware test loops.)

### Runtime settings + console (`main/settings.hpp`, `main/console.hpp`)

Wifi credentials, `ledder_url`, `ota_url` and `console_pass` are runtime settings: NVS value if set, else the Kconfig compile-time default (so `sdkconfig.<name>` files are per-board factory defaults). Add a setting = one line in `settings_defs[]`; changes apply after reboot. One `esp_console` command table (each setting is its own command — `wifi_ssid <value>` sets, bare `wifi_ssid` shows — plus `list`/`unset`/`defaults`/`info`/`reboot`; add commands via `console_register()`) is exposed on the serial REPL (uart or usb-serial-jtag, per sdkconfig) and on tcp port 23 (`nc`/`telnet` compatible), the latter gated by the `console_pass` setting — while it is empty (the default), remote access is refused until a password is set via serial or baked in via `CONFIG_LEDSTREAM_CONSOLE_PASS`. Remote output works by pointing the tcp task's per-task stdio at the socket, so command handlers must use `printf`, not `ESP_LOG`.

### Universal led interface (`main/leds.hpp`)

Write-only cursor API implemented by exactly one backend, chosen by Kconfig mode:

- `leds_reset()` — cursor to 0 (ws2812 also applies the pixels-per-channel=0 → compiled-default fallback here)
- `leds_setNextPixel(r,g,b)` — write pixel, advance cursor (ws2812: wraps channels at `leds_pixels_per_channel`; hub75: wraps x/y)
- `leds_keepPixels(count, &r,&g,&b)` — QOIS_OP_PREVFRAME support: skip `count` pixels leaving previous-frame content, return the last kept color (decoder needs it to stay in sync with the encoder)
- `leds_clear()` — blank everything (stream start)
- `leds_show()` — latch the frame

Backends: `leds_ws2812.cpp` (FastLED, persistent `CRGB leds[CHANNELS][LEDS_PER_CHANNEL]` framebuffer) and `leds_hub75.cpp` (ESP32-HUB75-MatrixPanel-DMA; draws straight to the DMA display, plus a shadow buffer solely because the DMA library can't read pixels back for `leds_keepPixels`). The hub75 `DOUBLE_BUFFERING` option is disabled and now also **incompatible with PREVFRAME** (the back buffer holds frame N-2).

### Streamers

- **`ledstreamer_http.hpp`** — the normal path. Connects to `LEDDER_URL/<MAC>` and reads a never-ending HTTP response. The server's `Mode` response header selects: `0` live-stream, `1` record (bytes are both decoded and written to flash via `fileserver`), `2` replay-from-flash. Modes 0/1 call `qois_resetStream()` because ledder resets its encoder state per connection.
- **`ledstreamer_flash.hpp`** — loops `/littlefs/stream.bin`; every replay restart calls `qois_resetStream()` (a recording starts at the beginning of a connection, so this is exact).
- `ota.hpp` — esp_https_ota against `FIRMWARE_UPGRADE_URL`; `timing.hpp` — frames carry a server-side display timestamp; the esp syncs a local offset to it and waits each frame to the right moment. (An old UDP streamer was removed in July 2026 — incompatible with the stream dialect: lossy transport + mid-stream resync vs. persistent decoder state. Don't revive from git history without ledder resetting its encoder per frame.)

### QOIS decoder (`main/qois.hpp`)

Byte-oriented streaming decoder for ledder's QOIS format (a [QOI](https://qoiformat.org) adaptation). Feed bytes to `qois_decodeBytes()`; each frame has a 6-byte header (frame byte-length 2B LE — frames cap at 64 KiB; pixels-per-channel 2B, 0 = use compiled default; display timestamp 2B ms). When a frame completes, `qois_show()` waits for its timestamp, latches the leds and resets per-frame state.

**Decoder contract — differs from stock QOI, keep in lockstep with ledder's `DisplayQOIS.ts`** (full comment block at the top of `qois.hpp`):

- The led framebuffer and the 64-color index **persist across frames**; `qois_resetStream()` zeroes both and must be called wherever a stream starts from its beginning (new connection, each flash replay loop). `qois_reset()` is per-frame only.
- The previous-pixel state resets to black each frame.
- The color index is updated **only by DIFF/LUMA/RGB ops**. RUN, INDEX and PREVFRAME must not touch it (stock QOI updates after every op — that desyncs from the encoder's persistent index).
- `QOIS_OP_PREVFRAME` (0xff, the never-emitted QOI_OP_RGBA byte): keep the next N pixels from the previous frame, 2-byte LE count; previous-pixel becomes the last kept pixel. Static content collapses to ~9 bytes/frame.
- Deltas use 8-bit wraparound (255→0 encodes as +1); reconstruct with wrapping uint8 adds.
- RGBA is never emitted; the stream is opaque RGB (ledder blends alpha away before encoding).

## Host-side decoder test (`test/host/`)

```sh
make -C test/host test
```

Compiles the **real** `main/qois.hpp` and the **real** `main/leds_ws2812.cpp` on the host (only FastLED and the esp glue are stubbed in `test/host/stub/`), feeds recorded ledder streams byte-by-byte, and compares the led buffer bit-exactly after every frame — in 1-channel and 8-channel configurations (the latter exercises PREVFRAME runs crossing channel boundaries), with flash-replay-style loop restarts. Run this after any change to `qois.hpp` or the ws2812 cursor logic.

Test vectors (`test/host/vectors/`) are generated by ledder's `tools/qois/qois-frames.mjs` (`capture` then `dump [ppc]`); a `.qois` only matches its *own* `.bin` because animations are randomized per capture. See `test/host/README.md`.

Note the byte-feeding quirk: `qois_decodeBytes` only "shows" a completed frame when the *next* byte arrives, so the test checks `qois_frame_bytes_left == 0` after each byte instead.

## Components

- **`components/ESP32-HUB75-MatrixPanel-DMA`** (git submodule, fork of [mrcodetastic's library](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)): upstream v3.0.12 plus a single backport commit fixing compilation on IDF 4.4.8 for esp32s3. The library bypasses the ESP-IDF I2S/RMT drivers entirely — original ESP32 pokes I2S peripheral registers via `soc/` structs, S3 drives LCD_CAM through `hal/lcd_ll.h` + private `esp_private/gdma.h`.
- **`components/FastLED-idf`** (vendored): FastLED 3.3.2 port (samguyer lineage) targeting IDF 4.x, using the **legacy RMT driver**. Only consumer is `leds_ws2812.cpp` (tiny surface: `CRGB`, `addLeds<WS2811>`, `show`, `clear`, `setMaxPowerInVoltsAndMilliamps`).
- `joltwallet/littlefs` managed component (pinned in `main/idf_component.yml`, which also pins `idf: 4.4.8`).
- Root `CMakeLists.txt` appends `-DNO_CIE1931=1` globally and lists the components explicitly.

## Upgrading ESP-IDF (investigated July 2026, not yet done)

- **The HUB75 library is not the blocker.** It contains `ESP_IDF_VERSION_MAJOR == 5` code paths (and a ≥5.4 GDMA guard) and builds against IDF 5.x upstream; because it programs registers directly it is unaffected by the legacy-driver removals. For IDF 6 the `== 5` guards need to become `>= 5` and the private GDMA calls need a test compile.
- **FastLED-idf is the blocker**: the legacy RMT driver is deprecated through IDF 5 and removed in IDF 6. Replace with upstream FastLED (RMT5/led_strip based; reported to flicker more under Wi-Fi load) or Espressif's `led_strip` component (would need manual power limiting to replace `setMaxPowerInVoltsAndMilliamps`).
- Small app fixes: `ethernet.cpp` (`esp_eth_mac_new_esp32` signature / `eth_esp32_emac_config_t` split in 5.0), `ota.hpp` (`esp_ota_get_app_description` → `esp_app_get_description`), bump littlefs, regenerate sdkconfigs, update `dockerrun` tag and the `idf_component.yml` pin.
- Suggested route: 4.4.8 → 5.5 first (HUB75 works as-is, swap FastLED, two app-code fixes), then 6.0 as a small second step.

## Conventions

- Header-only modules with `inline` + file-scope `static` state; `IRAM_ATTR` on every function in the decode/display hot path.
- All Kconfig symbols are prefixed `LEDSTREAM_` and live in `main/Kconfig.projbuild` under one menu.
- When touching the QOIS dialect: change ledder's `DisplayQOIS.ts` and `main/qois.hpp` together, update both DECODER CONTRACT comment blocks, regenerate test vectors and keep `make -C test/host test` bit-exact.
