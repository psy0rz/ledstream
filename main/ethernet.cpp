//
// Created by psy on 1/17/23.
//Copied and stripped from IDF basic ethernet example
#include "ethernet.h"

#include <string.h>
#include <driver/gpio.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "ethernet";



/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
       break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        ESP_LOGI(TAG, "huh?");

        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
}


void ethernet_init(void) {
//    vTaskDelay(1000/portTICK_PERIOD_MS);

//    // Initialize TCP/IP network interface (should be called only once in application)
//    ESP_ERROR_CHECK(esp_netif_init());
//    // Create default event loop that running in background
//    ESP_ERROR_CHECK(esp_event_loop_create_default());



#if CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET
    // Create new default instance of esp-netif for Ethernet
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&cfg);

    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_LEDSTREAM_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_LEDSTREAM_ETH_PHY_RST_GPIO;
    mac_config.smi_mdc_gpio_num = CONFIG_LEDSTREAM_ETH_MDC_GPIO;
    mac_config.smi_mdio_gpio_num = CONFIG_LEDSTREAM_ETH_MDIO_GPIO;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config);
#if CONFIG_LEDSTREAM_ETH_PHY_IP101
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
#elif CONFIG_LEDSTREAM_ETH_PHY_RTL8201
    esp_eth_phy_t *phy = esp_eth_phy_new_rtl8201(&phy_config);
#elif CONFIG_LEDSTREAM_ETH_PHY_LAN87XX
    esp_eth_phy_t *phy = esp_eth_phy_new_lan87xx(&phy_config);


    gpio_set_direction(static_cast<gpio_num_t>(CONFIG_LEDSTREAM_ETH_EXT_CLOCK_ACTIVATE), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(CONFIG_LEDSTREAM_ETH_EXT_CLOCK_ACTIVATE), 1);


#elif CONFIG_LEDSTREAM_ETH_PHY_DP83848
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
#elif CONFIG_LEDSTREAM_ETH_PHY_KSZ8041
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8041(&phy_config);
#elif CONFIG_LEDSTREAM_ETH_PHY_KSZ8081
    esp_eth_phy_t *phy = esp_eth_phy_new_ksz8081(&phy_config);
#endif
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));
    /* attach Ethernet driver to TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
#endif //CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET


    // Register user defined event handers
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* start Ethernet driver state machine */
#if CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
#endif // CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET



}
