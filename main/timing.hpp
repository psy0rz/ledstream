//
// Created by psy on 2/6/25.
//

#ifndef TIMING_HPP
#define TIMING_HPP
#include <timing.hpp>

static esp_timer_handle_t oneshot_timer;

static TaskHandle_t timing_source_task_handle = NULL;

inline void timer_callback(void* arg)
{
    if (timing_source_task_handle)
    {
        xTaskNotifyGive(timing_source_task_handle);
    }
}


//Use esp timer to wait until specified time in ms.
//Note that this time is the remote time and comes from ledstreamer, so we try to sync up with it.
int64_t timing_last_until_us = 0;
int64_t timing_last_time_us=0;
int64_t lag=0;

bool timing_should_reset=true;

inline void timing_reset()
{
timing_should_reset=true;

}

inline void timing_wait_until_us(int64_t until_us)
{
    if (timing_should_reset)
    {
        timing_should_reset=false;
    }
    else
    {
        //how long we should wait
        int64_t wait_time_us = until_us - timing_last_until_us;


        //how much time is already gone (due to processing)
        int64_t time_used=esp_timer_get_time()-timing_last_time_us;

        //how long should we still wait?
        int64_t wait_time_left=wait_time_us-time_used;

        // ESP_LOGI("timing", "waittime %lld uS, used %lld uS, left %lld uS", wait_time_us,time_used, wait_time_left);


        if (wait_time_left>0 && wait_time_left<2000000)
        {
            // ESP_LOGI("timing", "timing wait until us %d", until_us);
            timing_source_task_handle = xTaskGetCurrentTaskHandle();
            ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, wait_time_left));
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            // ESP_LOGI("timing", "done");
        }
        else
        {
            //lag 1 ms extra
            // timing_last_until_us=timing_last_time_us+100000;
            // timing_source_task_handle = NULL;
            // timing_last_time_us=timing_last_time_us-1000;
        }
    }


    timing_last_until_us = until_us;
    timing_last_time_us=esp_timer_get_time();

}


inline void timing_init()
{
    const esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        .name = "oneshot_timer"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &oneshot_timer));
    // ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, timer_interval)); // Start timer initially
}


#endif //TIMING_HPP
