#ifndef utils_hpp
#define utils_hpp

#include "esp_log.h"

//log OTA download progress, but only when the percentage actually changed
inline void progress_bar(int percentage) {
    static int last_percentage = -1;

    if (last_percentage == percentage)
        return;

    last_percentage = percentage;

    ESP_LOGI("OTA", "updating.. %d%%", percentage);
}

#endif
