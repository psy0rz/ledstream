#ifndef utils_hpp
#define utils_hpp

#include "esp_log.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "leds.hpp"

//defined in the ledstreamers (included after us in main.cpp)
inline void ledstreamer_http_stop();
inline void ledstreamer_flash_stop();

//prepare for going down: blank the display and leave it blanked. ws2812s and hub75
//panels keep displaying whatever was latched last, so the frame has to be cleared
//while we're still running. the delay gives the led hardware time to latch the
//cleared frame and pending log/console output time to flush.
inline void stop_streaming_and_blank() {
    //stop both streamers first, otherwise a decoded frame draws over the blanked display
    ledstreamer_http_stop();
    ledstreamer_flash_stop();

    leds_clear();
    leds_show();
    vTaskDelay(pdMS_TO_TICKS(250));
}

//blank the display and restart.
inline void reboot() {
    stop_streaming_and_blank();
    esp_restart();
}

//blank the display and deep sleep for the given number of minutes. the rtc timer
//wakeup restarts the firmware from the top (same as a reboot), so nothing needs to
//be restored on wake.
inline void deep_sleep(int minutes) {
    stop_streaming_and_blank();

    esp_sleep_enable_timer_wakeup((uint64_t) minutes * 60 * 1000000ULL);
    esp_deep_sleep_start();
}

//log OTA download progress, but only when the percentage actually changed
inline void progress_bar(int percentage) {
    static int last_percentage = -1;

    if (last_percentage == percentage)
        return;

    last_percentage = percentage;

    ESP_LOGI("OTA", "updating.. %d%%", percentage);
}

#endif
