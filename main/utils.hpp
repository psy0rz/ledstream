#ifndef utils_h
#define utils_h

#include <cstdint>
#include <esp_mac.h>


signed long diffUnsignedLong(unsigned long first, unsigned long second) {
    unsigned long abs_diff = (first > second) ? (first - second) : (second - first);
    return (first > second) ? (signed long) abs_diff : -(signed long) abs_diff;
}

int diff16(uint16_t first, uint16_t second) {
    uint16_t abs_diff = (first > second) ? (first - second) : (second - first);
    return (first > second) ? (int16_t) abs_diff : -(int16_t) abs_diff;
}

void wificheck() {
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


void get_mac_address(char *mac_str) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}


#endif