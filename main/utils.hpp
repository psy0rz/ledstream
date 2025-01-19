#ifndef utils_hpp
#define utils_hpp

#include <cstdint>
#include <esp_attr.h>
#include <esp_mac.h>
#include <esp_timer.h>


inline signed long diffUnsignedLong(unsigned long first, unsigned long second) {
    unsigned long abs_diff = (first > second) ? (first - second) : (second - first);
    return (first > second) ? (signed long) abs_diff : -(signed long) abs_diff;
}

inline int diff16(uint16_t first, uint16_t second) {
    uint16_t abs_diff = (first > second) ? (first - second) : (second - first);
    return (first > second) ? (int16_t) abs_diff : -(int16_t) abs_diff;
}

inline void wificheck() {
//    if (WiFi.status() == WL_CONNECTED)
//      return;
//
//    //    Serial.printf("Attempting to connect to WPA SSID: %s\n", ssid);
//    ESP_LOGI(UDPBUFFER_TAG, "Connecting to wifi...");
//    while (WiFi.status() != WL_CONNECTED) {
//      yield();
//      notify(CRGB::Red, 125, 250);
//    }
//    ESP_LOGI(UDPBUFFER_TAG, "Wifi connected.");

    //    Serial.printf("MDNS mdns name is %s\n",
    //    ArduinoOTA.getHostname().c_str()); Serial.println(WiFi.localIP());

    //    timeSync.begin();
}


inline void get_mac_address(char *mac_str) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


inline unsigned long IRAM_ATTR ms()
{
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

inline bool duty_cycle(unsigned long on, unsigned long total, unsigned long starttime = 0) {
    if (!starttime)
        return ((ms() % total) < on);
    else
        return (((ms() - starttime) % total) < on);
}


inline void progress_bar(int percentage) {
//     static int last_percentage = -1;
//
//     if (last_percentage == percentage)
//         return;
//
//     last_percentage = percentage;
//
//     ESP_LOGI("progress", "%d%%", percentage);
//
// #ifdef CONFIG_LEDSTREAM_MODE_WS2812
//     const int bar_length = 8;
//     int num_leds_lit = ((percentage * bar_length) / 100);
// //        static int flash_state = 0;
//
//     for (int i = 0; i < bar_length; i++) {
//         if (i < num_leds_lit) {
//             leds[0][i] = CRGB::DarkGreen;
//         } else {
//             leds[0][i] = CRGB::DarkRed;
//         }
//     }
//     FastLED.show();
}


#endif