#include "wifi.hpp"
#include "ethernet.h"
#include "ota.hpp"

#include "leds.hpp"

#include "qois.hpp"
// #include "ledstreamer_udp.hpp"

#include "ledstreamer_http.hpp"
#include "fileserver.hpp"

static const char* MAIN_TAG = "main";


OTAUpdater ota_updater = OTAUpdater();

#define MONITOR_TASK_PERIOD_MS 1000

static TaskStatus_t *prevTaskArray = NULL;
static uint32_t prevTotalRunTime = 0;
void monitor_task()
{
    while (1) {
        UBaseType_t numTasks = uxTaskGetNumberOfTasks();
        TaskStatus_t *taskArray = (TaskStatus_t *)pvPortMalloc(numTasks * sizeof(TaskStatus_t));
        if (taskArray != NULL) {
            uint32_t totalRunTime;
            numTasks = uxTaskGetSystemState(taskArray, numTasks, &totalRunTime);
            ESP_LOGI(MAIN_TAG, "Task Name       | Priority | CPU Usage (%%) | Core ID");
            ESP_LOGI(MAIN_TAG, "----------------------------------------------------");
            if (prevTaskArray != NULL && prevTotalRunTime > 0) {
                for (UBaseType_t i = 0; i < numTasks; i++) {
                    for (UBaseType_t j = 0; j < numTasks; j++) {
                        if (strcmp(taskArray[i].pcTaskName, prevTaskArray[j].pcTaskName) == 0) {
                            uint32_t runTimeDiff = taskArray[i].ulRunTimeCounter - prevTaskArray[j].ulRunTimeCounter;
                            uint32_t totalRunTimeDiff = totalRunTime - prevTotalRunTime;
                            float cpuUsage = (totalRunTimeDiff > 0) ? (100.0 * runTimeDiff / totalRunTimeDiff) : 0;
                            ESP_LOGI(MAIN_TAG, "%-15s | %-8d | %.2f%%        | %d", taskArray[i].pcTaskName, taskArray[i].uxCurrentPriority, cpuUsage, taskArray[i].xCoreID);
                            break;
                        }
                    }
                }
            } else {
                ESP_LOGI(MAIN_TAG, "First sample, CPU usage since boot");
            }
            if (prevTaskArray != NULL) {
                vPortFree(prevTaskArray);
            }
            prevTaskArray = taskArray;
            prevTotalRunTime = totalRunTime;
        }
        vTaskDelay(pdMS_TO_TICKS(MONITOR_TASK_PERIOD_MS));
    }
}


extern "C" __attribute__((unused)) void app_main(void)
{
    ESP_LOGI(MAIN_TAG, "Starting ledstreamer...");


    //settle
    vTaskDelay(250 / portTICK_PERIOD_MS);


    //Initialize NVS
    ESP_LOGI(MAIN_TAG, "Prepare NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGI(MAIN_TAG, "Erasing NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP network interface (should be called only once in application)
    ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    fileserver_init();

    leds_init();

    wifi_init_sta();

#if CONFIG_LEDSTREAM_USE_INTERNAL_ETHERNET

     ethernet_init();
#endif
    // fileserver_start();

    //    wificheck();
    // ledstreamer_udp.begin(65000);
    //    auto lastTime = millis();
    //    ESP_LOGI(MAIN_TAG, "RAM left %lu", esp_get_free_heap_size());

    //main task
    ESP_LOGI(MAIN_TAG, "Start mainloop:");

    // xTaskCreate(ledstreamer_udp_task, "ledstreamer_udp_task", 4096, nullptr, 1, nullptr);

    ledstreamer_http_init();

    monitor_task();
}
