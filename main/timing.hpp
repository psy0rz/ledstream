//
// Created by psy on 2/6/25.
//

#ifndef TIMING_HPP
#define TIMING_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

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

//one-shot playout delay: how far behind "instant" we start the very first frame of a
//stream. Since the wait below is purely differential (each frame is scheduled relative
//to the previous one, not to an absolute clock), this initial offset just shifts the
//whole stream's display schedule later by a fixed amount -- it doesn't accumulate -- and
//gives that much slack to absorb network jitter/stalls before a frame's wait goes
//negative (late).
#define TIMING_PLAYOUT_DELAY_US (150*1000)

int64_t timing_last_until_us = 0;
int64_t timing_last_time_us=0;
int64_t lag=0;

bool timing_should_reset=true;

inline void timing_reset()
{
timing_should_reset=true;

}

//stats for the console 'stats' command: min/max/avg of wait_time_left, accumulated
//since the last read and reset by timing_stats_get()
static int64_t timing_stat_min_us = INT64_MAX;
static int64_t timing_stat_max_us = INT64_MIN;
static int64_t timing_stat_sum_us = 0;
static int64_t timing_stat_count = 0;
static int64_t timing_stat_late_count = 0; //frames where wait_time_left < 0 (we were already behind)

inline void timing_stats_reset()
{
    timing_stat_min_us = INT64_MAX;
    timing_stat_max_us = INT64_MIN;
    timing_stat_sum_us = 0;
    timing_stat_count = 0;
    timing_stat_late_count = 0;
}

//returns false if no frames were timed since the last call
inline bool timing_stats_get(int64_t *min_us, int64_t *max_us, int64_t *avg_us, int64_t *count, int64_t *late_count)
{
    if (timing_stat_count == 0)
        return false;
    *min_us = timing_stat_min_us;
    *max_us = timing_stat_max_us;
    *avg_us = timing_stat_sum_us / timing_stat_count;
    *count = timing_stat_count;
    *late_count = timing_stat_late_count;
    timing_stats_reset();
    return true;
}

inline void timing_wait_until_us(int64_t until_us)
{
    if (timing_should_reset)
    {
        timing_should_reset=false;

        //build up the initial playout cushion before showing the first frame
        timing_source_task_handle = xTaskGetCurrentTaskHandle();
        ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, TIMING_PLAYOUT_DELAY_US));
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
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

        if (wait_time_left < timing_stat_min_us) timing_stat_min_us = wait_time_left;
        if (wait_time_left > timing_stat_max_us) timing_stat_max_us = wait_time_left;
        timing_stat_sum_us += wait_time_left;
        timing_stat_count++;
        if (wait_time_left < 0) timing_stat_late_count++;


        if (wait_time_left>0 && wait_time_left<2000000)
        {
            timing_source_task_handle = xTaskGetCurrentTaskHandle();
            ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, wait_time_left));
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }


    timing_last_until_us = until_us;
    timing_last_time_us=esp_timer_get_time();

}


inline void timing_init()
{
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &timer_callback;
    timer_args.name = "oneshot_timer";

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &oneshot_timer));
}


#endif //TIMING_HPP
