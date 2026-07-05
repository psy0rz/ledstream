/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#ifndef LEDSTREAM_WIFI_HPP
#define LEDSTREAM_WIFI_HPP

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"


#include "lwip/err.h"
#include "lwip/sys.h"

#include "settings.hpp"




/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *WIFI_TAG = "wifistuff";


volatile bool wifi_disconnected = true;


//scan all channels ourselves with a longer per-channel dwell than the driver's
//internal connect-scan (~120ms, not tunable), so we don't miss beacons from the
//strongest AP and end up on a weak one. SCAN_DONE picks the best BSSID and connects.
static void wifi_scan_for_best_ap() {
    wifi_scan_config_t scan;
    memset(&scan, 0, sizeof(scan));
    scan.ssid = (uint8_t *) settings_get("wifi_ssid");  //only report our own SSID
    scan.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan.scan_time.active.min = 200;
    scan.scan_time.active.max = 400;

    esp_err_t err = esp_wifi_scan_start(&scan, false);
    if (err != ESP_OK)
        ESP_LOGW(WIFI_TAG, "scan start failed: %s", esp_err_to_name(err));
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        //connect is done from wifi_init_sta(), after tx power is set
        wifi_disconnected = true;
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_disconnected = true;
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *) event_data;
        ESP_LOGI(WIFI_TAG, "disconnected (reason %d), rescanning for best AP", event->reason);
        wifi_scan_for_best_ap();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        uint16_t num = 0;
        esp_wifi_scan_get_ap_num(&num);
        //always call get_ap_records (even with num 0): it frees the driver's scan buffer
        wifi_ap_record_t *recs = (wifi_ap_record_t *) malloc(sizeof(wifi_ap_record_t) * (num ? num : 1));
        if (esp_wifi_scan_get_ap_records(&num, recs) == ESP_OK && num > 0) {
            int best = 0;
            for (int i = 0; i < num; i++) {
                ESP_LOGI(WIFI_TAG, "candidate %02x:%02x:%02x:%02x:%02x:%02x channel %d rssi %d",
                         recs[i].bssid[0], recs[i].bssid[1], recs[i].bssid[2],
                         recs[i].bssid[3], recs[i].bssid[4], recs[i].bssid[5],
                         recs[i].primary, recs[i].rssi);
                if (recs[i].rssi > recs[best].rssi)
                    best = i;
            }
            //lock onto the strongest BSSID; a disconnect triggers a fresh scan, so
            //we still fail over if this AP goes away later
            wifi_config_t cfg;
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            memcpy(cfg.sta.bssid, recs[best].bssid, sizeof(cfg.sta.bssid));
            cfg.sta.bssid_set = true;
            esp_wifi_set_config(WIFI_IF_STA, &cfg);
            ESP_LOGI(WIFI_TAG, "connecting to strongest AP %02x:%02x:%02x:%02x:%02x:%02x (rssi %d)",
                     recs[best].bssid[0], recs[best].bssid[1], recs[best].bssid[2],
                     recs[best].bssid[3], recs[best].bssid[4], recs[best].bssid[5], recs[best].rssi);
            esp_wifi_connect();
        } else {
            ESP_LOGW(WIFI_TAG, "SSID %s not found, rescanning", settings_get("wifi_ssid"));
            wifi_scan_for_best_ap();
        }
        free(recs);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_disconnected = false;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

        //log which AP/channel/signal we ended up on, to compare good vs bad boots
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
            ESP_LOGI(WIFI_TAG, "connected to bssid %02x:%02x:%02x:%02x:%02x:%02x channel %d rssi %d",
                     ap.bssid[0], ap.bssid[1], ap.bssid[2], ap.bssid[3], ap.bssid[4], ap.bssid[5],
                     ap.primary, ap.rssi);

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);


    }
}


//wait until we have an IP, or give up after timeout_ms (offline/flash-replay boot)
inline bool wifi_wait_connected(uint32_t timeout_ms) {
    if (s_wifi_event_group == NULL)  //wifi disabled (no SSID)
        return false;
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                           pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

inline void wifi_init_sta() {

    if (strlen(settings_get("wifi_ssid")) == 0) {
        ESP_LOGW(WIFI_TAG, "No SSID configured, wifi disabled ('set wifi_ssid ...' on the console)");
        return;
    }

    ESP_LOGI(WIFI_TAG,"Initialize wifi...");


    s_wifi_event_group = xEventGroupCreate();

    //NU IN MAIN
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));


    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    strlcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), settings_get("wifi_ssid"),
            sizeof(wifi_config.sta.ssid));
    strlcpy(reinterpret_cast<char *>(wifi_config.sta.password), settings_get("wifi_pass"),
            sizeof(wifi_config.sta.password));

    if (strlen(settings_get("wifi_pass")) == 0)
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    else
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    //fallback only: AP selection is normally done by wifi_scan_for_best_ap(),
    //which scans with a longer dwell time and locks the config to the best bssid
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_LOGI(WIFI_TAG, "connecting to SSID:%s", wifi_config.sta.ssid);


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    //NOTE: 80 (20db, ~100mw) seems to make behave things badly. )
    //72 is 18db ~63mW
    //60 is 15db ~32mw
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(80));

    //scan only after tx power is set, so scan/auth/assoc never runs at the default 20dBm
    wifi_scan_for_best_ap();

    //make per-boot RF conditions visible: REDUCE_TX_POWER silently lowers tx power after a brownout reset
    int8_t txp = 0;
    esp_wifi_get_max_tx_power(&txp);
    ESP_LOGI(WIFI_TAG, "max tx power: %d (0.25dBm units), reset reason: %d", txp, esp_reset_reason());



    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

}

#endif
