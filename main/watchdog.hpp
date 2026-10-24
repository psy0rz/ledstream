
#ifndef LEDSTREAM_WATCHDOG_HPP
#define LEDSTREAM_WATCHDOG_HPP

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "soc/rtc_wdt.h"

//RTC watchdog: the only watchdog that lives in the RTC power domain and runs off the
//RTC slow clock, so it keeps counting when a brownout or supply glitch wedges the cpu,
//the plls or the panic handler. On timeout it resets the whole chip, like pulling EN.
//
//Nothing feeds it for us. The bootloader arms it (CONFIG_BOOTLOADER_WDT_TIME_MS) and,
//with CONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=y, hands it over still running -- from
//then on this file owns it: reconfigure to the runtime timeout and feed it forever.
//
//IDF 5 note: soc/rtc_wdt.h is gone there, this becomes hal/wdt_hal.h.

static const char* WATCHDOG_TAG = "watchdog";

//time without a feed before the chip resets. Every feed reloads the counter to the full
//timeout, so nothing accumulates -- but the ms are converted to ticks using the *nominal*
//rtc slow clock rate while the feed interval is timed off the accurate main clock, so the
//real timeout can come out shorter than configured. 4x headroom covers that.
#define WATCHDOG_TIMEOUT_MS 2000
#define WATCHDOG_FEED_INTERVAL_MS 500

//flash erases run with the cache disabled: no task on either core runs, so nobody feeds.
//Erasing the 1M app partition takes seconds, hence this while an ota is in flight.
#define WATCHDOG_OTA_TIMEOUT_MS 60000

//above the streamer tasks (prio 10): the feed must not be starved by the display work
//itself, only by a real hang.
#define WATCHDOG_TASK_PRIO 20

inline void watchdog_set_timeout_ms(uint32_t timeout_ms)
{
    rtc_wdt_protect_off();
    rtc_wdt_set_time(RTC_WDT_STAGE0, timeout_ms);
    rtc_wdt_feed();
    rtc_wdt_protect_on();
}

//widen the timeout for as long as this object lives, then put it back. For code that
//blocks every task for longer than the normal timeout, like an ota flash erase.
class WatchdogTimeoutOverride
{
public:
    explicit WatchdogTimeoutOverride(uint32_t timeout_ms)
    {
        watchdog_set_timeout_ms(timeout_ms);
    }

    ~WatchdogTimeoutOverride()
    {
        watchdog_set_timeout_ms(WATCHDOG_TIMEOUT_MS);
    }
};

[[noreturn]] inline void watchdog_feed_task(void* args)
{
    while (true)
    {
        rtc_wdt_feed();
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_FEED_INTERVAL_MS));
    }
}

//call as early as possible in app_main: until this runs the bootloader timeout is still
//in effect, and nothing is feeding at all.
inline void watchdog_init()
{
    ESP_LOGI(WATCHDOG_TAG, "Arming rtc watchdog: reset after %d ms without a feed", WATCHDOG_TIMEOUT_MS);

    rtc_wdt_protect_off();
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_SYSTEM);
    rtc_wdt_set_time(RTC_WDT_STAGE0, WATCHDOG_TIMEOUT_MS);
    rtc_wdt_feed();
    rtc_wdt_enable();
    rtc_wdt_protect_on();

    //pinned to core 0, where the streaming and display tasks live, so a scheduler that
    //stops running there is caught
    xTaskCreatePinnedToCore(&watchdog_feed_task, "watchdog_feed", 2048, nullptr, WATCHDOG_TASK_PRIO, nullptr, 0);
}

#endif
