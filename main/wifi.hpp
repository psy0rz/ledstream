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
#include "sdkconfig.h"


#include "lwip/err.h"
#include "lwip/sys.h"


#include "ota.hpp"
#include "leds.hpp"


/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *WIFI_TAG = "wifistuff";


bool wifi_disconnected = true;

// static void wifi_blinker(void *pvParameter) {
//     while (1) {
//
//         //XXX
//         // while (wifi_disconnected)
//         //     blink_led(CRGB(50, 0, 0), 250, 500);
//         //
//         // status_led(CRGB(0, 50, 0));
//         // while (!wifi_disconnected)
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//
//     }
//
// }


static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        wifi_disconnected = true;
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_disconnected = true;
        esp_wifi_connect();
        ESP_LOGI(WIFI_TAG, "retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_disconnected = false;
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(WIFI_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
         ota_updater.init();


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


#ifndef CONFIG_LEDSTREAM_WIFI_SSID
    ESP_LOGW(WIFI_TAG,"No SSID specified, wifi disabled");
    return ;
#else
    ESP_LOGI(WIFI_TAG,"Initialize wifi...");

//    xTaskCreate(&wifi_blinker, "wifi_blinker", 1024, NULL, 5, NULL);


    s_wifi_event_group = xEventGroupCreate();

    //NU IN MAIN
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));


    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    strcpy(reinterpret_cast<char *>(wifi_config.sta.ssid), CONFIG_LEDSTREAM_WIFI_SSID);
    strcpy(reinterpret_cast<char *>(wifi_config.sta.password), CONFIG_LEDSTREAM_WIFI_PASS);

    if (strlen(CONFIG_LEDSTREAM_WIFI_PASS) == 0)
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    else
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_LOGI(WIFI_TAG, "connecting to SSID:%s password:%s", wifi_config.sta.ssid, wifi_config.sta.password);


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);

    //NOTE: 80 (20db, ~100mw) seems to make behave things badly. )
    //72 is 18db ~63mW
    //60 is 15db ~32mw
    esp_wifi_set_max_tx_power(60);

    //make per-boot RF conditions visible: REDUCE_TX_POWER silently lowers tx power after a brownout reset
    int8_t txp = 0;
    esp_wifi_get_max_tx_power(&txp);
    ESP_LOGI(WIFI_TAG, "max tx power: %d (0.25dBm units), reset reason: %d", txp, esp_reset_reason());



    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");


#endif
}
