#ifndef utils_h
#define utils_h

#include <cstdint>


signed long diffUnsignedLong(unsigned long first, unsigned long second) {
    unsigned  long abs_diff = (first > second) ? (first - second): (second - first);
    return (first > second) ? (signed long)abs_diff : -(signed long)abs_diff;
}

int diff16(uint16_t first, uint16_t second)
{
    uint16_t abs_diff = (first > second) ? (first - second): (second - first);
    return (first > second) ? (int16_t )abs_diff : -(int16_t )abs_diff;
}

void wificheck() {
//    if (WiFi.status() == WL_CONNECTED)
//      return;
//
//    //    Serial.printf("Attempting to connect to WPA SSID: %s\n", ssid);
//    ESP_LOGI(TAG, "Connecting to wifi...");
//    while (WiFi.status() != WL_CONNECTED) {
//      yield();
//      notify(CRGB::Red, 125, 250);
//    }
//    ESP_LOGI(TAG, "Wifi connected.");

    //    Serial.printf("MDNS mdns name is %s\n",
    //    ArduinoOTA.getHostname().c_str()); Serial.println(WiFi.localIP());

    //    multicastSync.begin();
}

#endif