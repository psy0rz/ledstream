#ifndef LEDSTREAM_REMOTE_CONFIG_HPP
#define LEDSTREAM_REMOTE_CONFIG_HPP

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "nvs.h"

#include "settings.hpp"
#include "console.hpp"
#include "wifi.hpp"

// Provisioning over http: downloads the text file at the 'config_url' setting once a
// minute. Every line is a console command (see console.hpp); lines that are empty or
// start with '#' are ignored. Example file:
//
//     # hall installation
//     wifi_ssid venue-iot
//     wifi_pass hunter2
//     ledder_url http://10.0.0.5:3000/stream
//
// Nothing happens while the file is unchanged: a hash of the last applied content is
// kept in nvs, and only a differing hash triggers a run. After running the commands the
// device reboots, because settings only take effect at boot.
//
// The hash is stored *before* the commands run, so a command that fails (or crashes the
// device) can never turn into a reboot loop -- the file has to change again before we
// retry. Combined with the fallback wifi settings this makes a device recoverable
// without a serial cable: it joins the fallback network and picks up its real config
// from the file there.

static const char *REMOTE_CONFIG_TAG = "remote_config";

#define REMOTE_CONFIG_MAX_SIZE 4096
#define REMOTE_CONFIG_INTERVAL_MS 60000

//how long the first check waits for wifi before trying anyway (ethernet-only boards
//never get the wifi bit, and a failed download just retries a minute later)
#define REMOTE_CONFIG_WIFI_TIMEOUT_MS 30000

//nvs key in the settings namespace, written by us instead of by the user, so it is
//deliberately not a settings_defs[] entry
#define REMOTE_CONFIG_HASH_KEY "config_hash"

static char remote_config_text[REMOTE_CONFIG_MAX_SIZE];

//downloads config_url into remote_config_text, returns its length or -1 on any failure
static int remote_config_download(const char *url) {
    esp_http_client_config_t http_config = {};
    http_config.url = url;
    http_config.timeout_ms = 10000;
    //verify https servers against the idf certificate bundle (ignored for plain http)
    http_config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if (client == NULL)
        return -1;

    int length = -1;

    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGW(REMOTE_CONFIG_TAG, "cannot connect to %s", url);
    } else if (esp_http_client_fetch_headers(client) < 0) {
        ESP_LOGW(REMOTE_CONFIG_TAG, "no response from %s", url);
    } else if (esp_http_client_get_status_code(client) != 200) {
        ESP_LOGW(REMOTE_CONFIG_TAG, "http status %d for %s",
                 esp_http_client_get_status_code(client), url);
    } else if (esp_http_client_get_content_length(client) >= REMOTE_CONFIG_MAX_SIZE) {
        ESP_LOGE(REMOTE_CONFIG_TAG, "config file is larger than %d bytes", REMOTE_CONFIG_MAX_SIZE - 1);
    } else {
        length = 0;
        while (length < REMOTE_CONFIG_MAX_SIZE - 1) {
            int received = esp_http_client_read(client, remote_config_text + length,
                                                REMOTE_CONFIG_MAX_SIZE - 1 - length);
            if (received <= 0)
                break;
            length += received;
        }
        remote_config_text[length] = 0;

        //chunked responses have no content length, so catch an oversized one here
        if (length == REMOTE_CONFIG_MAX_SIZE - 1) {
            ESP_LOGE(REMOTE_CONFIG_TAG, "config file is larger than %d bytes", REMOTE_CONFIG_MAX_SIZE - 1);
            length = -1;
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return length;
}

//FNV-1a, only used to notice that the file changed
static uint64_t remote_config_hash(const char *text) {
    uint64_t hash = 14695981039346656037ULL;
    for (const char *c = text; *c; c++) {
        hash ^= (uint8_t) *c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

//runs every command line in text (destroys it: strtok)
static void remote_config_apply(char *text) {
    for (char *line = strtok(text, "\r\n"); line != NULL; line = strtok(NULL, "\r\n")) {
        while (*line == ' ' || *line == '\t')
            line++;
        if (*line == 0 || *line == '#')
            continue;

        ESP_LOGI(REMOTE_CONFIG_TAG, "> %s", line);
        console_run_line(line);
    }
}

[[noreturn]] static void remote_config_task(void *args) {
    //check as soon as we have an ip, instead of guessing a delay
    if (!wifi_wait_connected(REMOTE_CONFIG_WIFI_TIMEOUT_MS))
        ESP_LOGI(REMOTE_CONFIG_TAG, "no wifi (yet), checking anyway");

    while (true) {
        int length = remote_config_download(settings_get("config_url"));

        if (length >= 0) {
            uint64_t hash = remote_config_hash(remote_config_text);
            uint64_t applied_hash = 0;
            nvs_get_u64(settings_nvs, REMOTE_CONFIG_HASH_KEY, &applied_hash);

            if (hash != applied_hash) {
                ESP_LOGW(REMOTE_CONFIG_TAG, "config file changed (%d bytes), applying", length);

                //remember it before running anything, so a broken file cannot loop
                nvs_set_u64(settings_nvs, REMOTE_CONFIG_HASH_KEY, hash);
                nvs_commit(settings_nvs);

                remote_config_apply(remote_config_text);

                ESP_LOGW(REMOTE_CONFIG_TAG, "config applied, rebooting");
                vTaskDelay(pdMS_TO_TICKS(250));
                esp_restart();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(REMOTE_CONFIG_INTERVAL_MS));
    }
}

inline void remote_config_init() {
    if (strlen(settings_get("config_url")) == 0) {
        ESP_LOGI(REMOTE_CONFIG_TAG, "no config_url set, remote config disabled");
        return;
    }
    ESP_LOGI(REMOTE_CONFIG_TAG, "checking %s every %d seconds",
             settings_get("config_url"), REMOTE_CONFIG_INTERVAL_MS / 1000);
    //lowest priority, like the ota task: a once-a-minute download must never compete
    //with the streamer/decoder tasks (priority 10) for cpu
    xTaskCreate(&remote_config_task, "remote_config", 8192, nullptr, tskIDLE_PRIORITY, nullptr);
}

#endif //LEDSTREAM_REMOTE_CONFIG_HPP
