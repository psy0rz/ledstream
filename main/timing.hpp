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
//
//The client adds no playout delay of its own: the server renders and sends the
//first ~500ms of frames as fast as it can, so the jitter cushion is the backlog
//sitting in the network pipe. The first frame is shown immediately and anchors
//the schedule; the burst frames then arrive early and each waits for its slot.
//
//The server never drops frames -- it keeps pushing until TCP back pressure stops
//it -- and frame timestamps are synthetic (always frame-interval apart). So when
//we fall behind (network too slow), the right move is to keep the original
//anchor and render as fast as frames arrive (effective wait 0) until we've
//caught back up to the schedule; we never re-anchor to "forgive" the delay.

//fixed anchor pair, set once per stream (see timing_should_reset below): all
//per-frame targets are computed relative to this pair, so per-frame scheduling
//jitter can never compound into long-term drift.
int64_t timing_anchor_local_us = 0;
int64_t timing_anchor_until_us = 0;

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

        //show the first frame immediately and anchor the stream's schedule here:
        //from now on every frame's target is computed relative to this fixed
        //(local, remote) pair, never re-derived from the previous frame's actual
        //(jittered) wake time. The server's initial fast-rendered burst arrives
        //"early" against this anchor and forms the jitter cushion in the pipe.
        timing_anchor_local_us = esp_timer_get_time();
        timing_anchor_until_us = until_us;
    }
    else
    {
        //absolute target for this frame, derived from the fixed stream anchor
        int64_t target_local_us = timing_anchor_local_us + (until_us - timing_anchor_until_us);

        //how long should we still wait?
        int64_t wait_time_left = target_local_us - esp_timer_get_time();

        // ESP_LOGI("timing", "left %lld uS", wait_time_left);

        //stats
        if (wait_time_left < timing_stat_min_us) timing_stat_min_us = wait_time_left;
        if (wait_time_left > timing_stat_max_us) timing_stat_max_us = wait_time_left;
        timing_stat_sum_us += wait_time_left;
        timing_stat_count++;
        if (wait_time_left < 0) timing_stat_late_count++;

        if (wait_time_left>0)
        {
            //wait for wait_time_left uS
            timing_source_task_handle = xTaskGetCurrentTaskHandle();
            ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, wait_time_left));
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
    }
}


inline void timing_init()
{
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &timer_callback;
    timer_args.name = "oneshot_timer";

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &oneshot_timer));
}


#endif //TIMING_HPP
