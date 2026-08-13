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


//point the station config at one specific network + AP. Called from SCAN_DONE,
//once we know which of the two configured SSIDs is actually in range.
static void wifi_use_network(const char *ssid, const char *pass, const uint8_t *bssid) {
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));

    strlcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), ssid, sizeof(wifi_config.sta.ssid));
    strlcpy(reinterpret_cast<char *>(wifi_config.sta.password), pass, sizeof(wifi_config.sta.password));

    if (strlen(pass) == 0)
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    else
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    //fallback only: AP selection is normally done by wifi_scan_for_best_ap(),
    //which scans with a longer dwell time and locks the config to the best bssid
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    if (bssid) {
        memcpy(wifi_config.sta.bssid, bssid, sizeof(wifi_config.sta.bssid));
        wifi_config.sta.bssid_set = true;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

//scan all channels ourselves with a longer per-channel dwell than the driver's
//internal connect-scan (~120ms, not tunable), so we don't miss beacons from the
//strongest AP and end up on a weak one. SCAN_DONE picks the best BSSID and connects.
//The scan is not SSID-filtered because we have to see both the primary and the
//fallback SSID to decide between them.
static void wifi_scan_for_best_ap() {
    wifi_scan_config_t scan;
    memset(&scan, 0, sizeof(scan));
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

        const char *primary_ssid = settings_get("wifi_ssid");
        const char *fallback_ssid = settings_get("wifi_ssid2");
        int best_primary = -1;
        int best_fallback = -1;

        if (recs && esp_wifi_scan_get_ap_records(&num, recs) == ESP_OK && num > 0) {
            for (int i = 0; i < num; i++) {
                const char *found_ssid = (const char *) recs[i].ssid;
                int *best = NULL;
                if (strlen(primary_ssid) > 0 && strcmp(found_ssid, primary_ssid) == 0)
                    best = &best_primary;
                else if (strlen(fallback_ssid) > 0 && strcmp(found_ssid, fallback_ssid) == 0)
                    best = &best_fallback;
                else
                    continue;  //some other network

                ESP_LOGI(WIFI_TAG, "candidate %s %02x:%02x:%02x:%02x:%02x:%02x channel %d rssi %d",
                         found_ssid,
                         recs[i].bssid[0], recs[i].bssid[1], recs[i].bssid[2],
                         recs[i].bssid[3], recs[i].bssid[4], recs[i].bssid[5],
                         recs[i].primary, recs[i].rssi);

                if (*best < 0 || recs[i].rssi > recs[*best].rssi)
                    *best = i;
            }
        }

        //the primary network wins whenever it is in range at all, however weak;
        //the fallback is only there to get an unprovisioned/moved device online
        bool use_fallback = (best_primary < 0);
        int best = use_fallback ? best_fallback : best_primary;

        if (best >= 0) {
            //lock onto the strongest BSSID; a disconnect triggers a fresh scan, so
            //we still fail over if this AP goes away later
            wifi_use_network(use_fallback ? fallback_ssid : primary_ssid,
                             settings_get(use_fallback ? "wifi_pass2" : "wifi_pass"),
                             recs[best].bssid);
            ESP_LOGI(WIFI_TAG, "connecting to strongest %s AP %02x:%02x:%02x:%02x:%02x:%02x (rssi %d)",
                     use_fallback ? "fallback" : "primary",
                     recs[best].bssid[0], recs[best].bssid[1], recs[best].bssid[2],
                     recs[best].bssid[3], recs[best].bssid[4], recs[best].bssid[5], recs[best].rssi);
            esp_wifi_connect();
        } else {
            if (strlen(fallback_ssid) > 0)
                ESP_LOGW(WIFI_TAG, "neither SSID %s nor %s found, rescanning", primary_ssid, fallback_ssid);
            else
                ESP_LOGW(WIFI_TAG, "SSID %s not found, rescanning", primary_ssid);
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

    bool has_primary = strlen(settings_get("wifi_ssid")) > 0;
    bool has_fallback = strlen(settings_get("wifi_ssid2")) > 0;

    if (!has_primary && !has_fallback) {
        ESP_LOGW(WIFI_TAG, "No SSID configured, wifi disabled ('wifi_ssid ...' on the console)");
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


    if (has_fallback)
        ESP_LOGI(WIFI_TAG, "SSID:%s, fallback SSID:%s",
                 settings_get("wifi_ssid"), settings_get("wifi_ssid2"));
    else
        ESP_LOGI(WIFI_TAG, "SSID:%s", settings_get("wifi_ssid"));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //placeholder until the scan tells us which network/AP to actually use
    if (has_primary)
        wifi_use_network(settings_get("wifi_ssid"), settings_get("wifi_pass"), NULL);
    else
        wifi_use_network(settings_get("wifi_ssid2"), settings_get("wifi_pass2"), NULL);
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
