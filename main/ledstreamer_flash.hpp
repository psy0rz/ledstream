//plays animation from flash in a loop

#ifndef LEDSTREAMER_FLASH_HPP
#define LEDSTREAMER_FLASH_HPP


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char* LEDSTREAMER_FLASH_TAG = "LEDSTREAMER_FLASH";

// Task handle and mutex for synchronization
static TaskHandle_t s_task_handle = NULL;
static TaskHandle_t s_caller_handle = NULL;
static SemaphoreHandle_t s_task_mutex = NULL;

// Task function
inline void ledstreamer_flash_task(void* arg)
{
    ESP_LOGI(LEDSTREAMER_FLASH_TAG, "Started");

    // Notify that the task has started
    xSemaphoreGive(s_task_mutex);

    uint32_t notification_value;
    while (!xTaskNotifyWait(0, 0, &notification_value, 0))
    {
        // Simulate task work
        ESP_LOGI(LEDSTREAMER_FLASH_TAG, "Task is running...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second

    }

    // Notify the stop function that the task is about to exit
    ESP_LOGI(LEDSTREAMER_FLASH_TAG, "Task stopping");
    xTaskNotifyGive(s_caller_handle);

    // Task cleanup
    vTaskDelete(NULL); // Delete itself
}

// Start the task
inline void ledstreamer_flash_start()
{
    // Create the mutex if it doesn't exist
    if (s_task_mutex == NULL)
    {
        s_task_mutex = xSemaphoreCreateMutex();
        if (s_task_mutex == NULL)
        {
            ESP_LOGE(LEDSTREAMER_FLASH_TAG, "Failed to create mutex");
            return;
        }
    }

    // Lock the mutex to protect s_task_handle
    xSemaphoreTake(s_task_mutex, portMAX_DELAY);

    if (s_task_handle != NULL)
    {
        ESP_LOGE(LEDSTREAMER_FLASH_TAG, "Task is already running");
        xSemaphoreGive(s_task_mutex);
        return;
    }

    // Create the task
    xTaskCreate(ledstreamer_flash_task, "ledstreamer_flash", 4096, NULL, 5, &s_task_handle);

    // Wait for the task to initialize
    xSemaphoreTake(s_task_mutex, portMAX_DELAY);

    // Unlock the mutex
    xSemaphoreGive(s_task_mutex);
}

// Stop the task and wait for it to exit
inline void ledstreamer_flash_stop()
{
    // Lock the mutex to protect s_task_handle
    xSemaphoreTake(s_task_mutex, portMAX_DELAY);


    if (s_task_handle == NULL)
    {
        ESP_LOGE(LEDSTREAMER_FLASH_TAG, "Task is not running");
        xSemaphoreGive(s_task_mutex);
        return;
    }

    s_caller_handle = xTaskGetCurrentTaskHandle();

    // Send a notification to the task to stop
    xTaskNotify(s_task_handle, 0, eNoAction);

    // Wait for the task to acknowledge that it has stopped
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    ESP_LOGI(LEDSTREAMER_FLASH_TAG, "Task has stopped");

    // Reset the task handle
    s_task_handle = NULL;

    // Unlock the mutex
    xSemaphoreGive(s_task_mutex);
}



#endif //LEDSTREAMER_FLASH_HPP
