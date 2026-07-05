#ifndef LEDSTREAM_SETTINGS_HPP
#define LEDSTREAM_SETTINGS_HPP

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "sdkconfig.h"

// Runtime settings: value from NVS if set, otherwise the compile-time Kconfig default.
// So sdkconfig.<board> files act as factory defaults and an empty NVS keeps working.
//
// To add a setting: add one line to settings_defs[] (NVS key max 15 chars) and read it
// with settings_get("key"). It automatically shows up in the console 'set' command.
//
// Values are cached at boot; a 'set' writes NVS but consumers pick it up after reboot.

#define SETTINGS_MAX_VALUE 128

struct settings_def_t {
    const char *key;
    const char *def;   //compile-time default (Kconfig)
    bool secret;       //mask value in listings
    const char *help;
};

static const settings_def_t settings_defs[] = {
        {"wifi_ssid",    CONFIG_LEDSTREAM_WIFI_SSID,            false, "wifi SSID (empty = wifi disabled)"},
        {"wifi_pass",    CONFIG_LEDSTREAM_WIFI_PASS,            true,  "wifi password"},
        {"ledder_url",   CONFIG_LEDSTREAM_LEDDER_URL,           false, "ledder stream url"},
        {"ota_url",      CONFIG_LEDSTREAM_FIRMWARE_UPGRADE_URL, false, "firmware upgrade url"},
        {"console_pass", CONFIG_LEDSTREAM_CONSOLE_PASS,         true,  "remote console password (empty = remote console disabled)"},
};
#define SETTINGS_COUNT (sizeof(settings_defs) / sizeof(settings_defs[0]))

static const char *SETTINGS_TAG = "settings";

static nvs_handle_t settings_nvs;
static char settings_values[SETTINGS_COUNT][SETTINGS_MAX_VALUE];
static bool settings_from_nvs[SETTINGS_COUNT];

inline int settings_index(const char *key) {
    for (int i = 0; i < (int) SETTINGS_COUNT; i++)
        if (strcmp(settings_defs[i].key, key) == 0)
            return i;
    return -1;
}

//call after nvs_flash_init()
inline void settings_init() {
    ESP_ERROR_CHECK(nvs_open("ledstream", NVS_READWRITE, &settings_nvs));

    for (int i = 0; i < (int) SETTINGS_COUNT; i++) {
        size_t len = SETTINGS_MAX_VALUE;
        if (nvs_get_str(settings_nvs, settings_defs[i].key, settings_values[i], &len) == ESP_OK) {
            settings_from_nvs[i] = true;
            ESP_LOGI(SETTINGS_TAG, "%s = %s (from nvs)", settings_defs[i].key,
                     settings_defs[i].secret ? "***" : settings_values[i]);
        } else {
            settings_from_nvs[i] = false;
            strlcpy(settings_values[i], settings_defs[i].def, SETTINGS_MAX_VALUE);
        }
    }
}

inline const char *settings_get(const char *key) {
    int i = settings_index(key);
    if (i < 0) {
        ESP_LOGE(SETTINGS_TAG, "unknown setting '%s'", key);
        return "";
    }
    return settings_values[i];
}

inline bool settings_set(const char *key, const char *value) {
    int i = settings_index(key);
    if (i < 0 || strlen(value) >= SETTINGS_MAX_VALUE)
        return false;
    if (nvs_set_str(settings_nvs, key, value) != ESP_OK)
        return false;
    nvs_commit(settings_nvs);
    strlcpy(settings_values[i], value, SETTINGS_MAX_VALUE);
    settings_from_nvs[i] = true;
    return true;
}

//back to the compile-time default
inline bool settings_erase(const char *key) {
    int i = settings_index(key);
    if (i < 0)
        return false;
    esp_err_t err = nvs_erase_key(settings_nvs, key);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return false;
    nvs_commit(settings_nvs);
    strlcpy(settings_values[i], settings_defs[i].def, SETTINGS_MAX_VALUE);
    settings_from_nvs[i] = false;
    return true;
}

#endif //LEDSTREAM_SETTINGS_HPP
