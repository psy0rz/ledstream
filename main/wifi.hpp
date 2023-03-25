/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi-config.h"


#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *WIFI_TAG = "wifistuff";


static void event_handler(void *arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(void) {


#ifndef WIFI_SSID
    ESP_LOGW(WIFI_TAG,"No SSID specified, wifi disabled");
    return ;
#else

    s_wifi_event_group = xEventGroupCreate();

//    ESP_ERROR_CHECK(esp_netif_init());
//
//    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));


    wifi_config_t wifi_config;
    memset(&wifi_config, 0 , sizeof(wifi_config_t));

    strcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), WIFI_SSID);
    strcpy(reinterpret_cast<char *>(wifi_config.sta.password), WIFI_PASS);
    wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_LOGI(WIFI_TAG, "connecting to SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);


    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
     * bits are set by event_handler() (see above) */
//    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
//                                           pdFALSE, pdFALSE, portMAX_DELAY);
//
//    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
//     * can test which event actually happened. */
//    if (bits & WIFI_CONNECTED_BIT) {
//        ESP_LOGI(WIFI_TAG, "connected to ap SSID:%s password:%s", WIFI_SSID,
//                 WIFI_PASS);
//    } else if (bits & WIFI_FAIL_BIT) {
//        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s",
//                 WIFI_SSID, WIFI_PASS);
//    } else {
//        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
//    }

#endif
}
