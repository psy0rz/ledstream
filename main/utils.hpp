#ifndef utils_hpp
#define utils_hpp

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "leds.hpp"

//defined in the ledstreamers (included after us in main.cpp)
inline void ledstreamer_http_stop();
inline void ledstreamer_flash_stop();

//blank the display and restart. the delay gives the led hardware time to latch
//the cleared frame (and pending log/console output time to flush) before we go down.
inline void reboot() {
    //stop both streamers first, otherwise a decoded frame draws over the blanked display
    ledstreamer_http_stop();
    ledstreamer_flash_stop();

    leds_clear();
    leds_show();
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_restart();
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
