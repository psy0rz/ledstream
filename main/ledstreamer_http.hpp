//connect to ledder via http and stream led animtion via qois. can also write animation to flash.
//
//Two tasks, decoupled by a FreeRTOS stream buffer:
// - the reader task owns the http connection and drains the socket as fast as
//   data arrives, so the jitter cushion lives in our own stream buffer instead
//   of pinning lwip/wifi driver rx buffers (which starves all other wifi
//   traffic once the pool runs out).
// - the consumer task takes bytes from the stream buffer and does the slow
//   work: flash writes (recording) and qois decoding, which paces itself by
//   sleeping until each frame's display time.
//When the stream buffer is full the reader blocks, which closes the TCP window
//and back-pressures the server, same as before -- just with a cushion we size
//ourselves.


#ifndef LEDSTREA_HTTP_HPP
#define LEDSTREA_HTTP_HPP

#include <fileserver.hpp>
#include <ledstreamer_flash.hpp>

#include "freertos/stream_buffer.h"

#include "qois.hpp"
#include "settings.hpp"

const char* LEDSTREAMER_HTTP_TAG = "ledstreamer_http";

//jitter cushion between socket and decoder (internal ram)
#define LEDSTREAMER_HTTP_BUFFER_SIZE (32 * 1024)
#define LEDSTREAMER_HTTP_CHUNK_SIZE 2048

char url[200];

fileserver_ctx* ledstreamer_http_file_ctx = nullptr;

bool stream_flashing = false;
bool stream_live = false;

bool http_connected = false;

//Mode response header of the current connection, -1 = not received
static volatile int stream_mode = -1;

static StreamBufferHandle_t stream_buffer = nullptr;

//single-writer counters for drain synchronization: only the reader task writes
//_produced, only the consumer task writes _consumed (32-bit stores are atomic
//on xtensa). Equal means the consumer is idle and the buffer is empty.
static volatile uint32_t stream_bytes_produced = 0;
static volatile uint32_t stream_bytes_consumed = 0;

//wait until the consumer has processed everything the reader put in the buffer
inline void stream_drain()
{
    while (stream_bytes_consumed != stream_bytes_produced)
        vTaskDelay(10 / portTICK_PERIOD_MS);
}

//takes bytes from the stream buffer and does the slow work: flash writes and
//qois decoding (which sleeps until each frame's display time).
[[noreturn]] inline void ledstreamer_http_consume_task(void* args)
{
    static uint8_t chunk[LEDSTREAMER_HTTP_CHUNK_SIZE];

    while (true)
    {
        size_t received = xStreamBufferReceive(stream_buffer, chunk, sizeof(chunk), portMAX_DELAY);

        if (stream_flashing)
        {
            if (ledstreamer_http_file_ctx == nullptr)
                ledstreamer_http_file_ctx = fileserver_open(true);
            fileserver_write(ledstreamer_http_file_ctx, chunk, received);
        }

        if (stream_live)
            qois_decodeBytes(chunk, received, 0);

        stream_bytes_consumed += received;
    }
}

inline void stream()
{
    if (wifi_disconnected)
        return;

    ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connecting: %s", url);

    stream_flashing = false;
    stream_live = false;
    stream_mode = -1;
    http_connected = false;

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 60000;
    config.event_handler = [](esp_http_client_event_t* evt)
        {
            switch (evt->event_id)
            {
            default:
                break;

            case HTTP_EVENT_ON_CONNECTED:
                ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Connected");
                http_connected = true;
                break;

            case HTTP_EVENT_ON_HEADER:
                //only record the mode here; acting on it happens in stream()
                //after the headers are in, so the reader task stays free to
                //drain the socket.
                if (strcmp(evt->header_key, "Mode") == 0)
                    stream_mode = atoi(evt->header_value);
                break;
            }
            return ESP_OK;
        };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        ESP_LOGE(LEDSTREAMER_HTTP_TAG, "Connection failed");
        esp_http_client_cleanup(client);
        http_connected = false;
        ledstreamer_flash_start();
        return;
    }

    if (esp_http_client_fetch_headers(client) >= 0)
    {
        //live streaming
        if (stream_mode == 0)
        {
            ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Live streaming");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ledstreamer_flash_stop();
            stream_drain();
            //the ledder encoder starts from a fresh state per connection
            qois_resetStream();
            timing_reset();
            stream_live = true;
        }
        //record
        else if (stream_mode == 1)
        {
            ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Recording");
            ledstreamer_flash_stop();
            stream_drain();
            //the ledder encoder starts from a fresh state per connection
            qois_resetStream();
            timing_reset();
            stream_flashing = true;
            stream_live = true;
        }
        //play
        else if (stream_mode == 2)
        {
            ledstreamer_flash_start();
        }
        else
        {
            ESP_LOGE(LEDSTREAMER_HTTP_TAG, "Unknown mode");
        }

        //drain the socket as fast as data arrives; the cushion accumulates in
        //the stream buffer and a full buffer back-pressures the server via TCP.
        //
        //esp_http_client_read never returns partial data: it loops until the
        //requested length is filled or a transport read times out. On a
        //low-bitrate stream that batches frames until the chunk fills (100s of
        //ms of display lag). So: block for the first byte with the long
        //timeout, then slurp whatever is already buffered locally with a ~zero
        //timeout so the read returns immediately instead of accumulating.
        static uint8_t chunk[LEDSTREAMER_HTTP_CHUNK_SIZE];
        while (true)
        {
            esp_http_client_set_timeout_ms(client, 60000);
            int received = esp_http_client_read(client, (char*)chunk, 1);
            if (received <= 0)
                break;

            esp_http_client_set_timeout_ms(client, 1);
            int more = esp_http_client_read(client, (char*)chunk + 1, sizeof(chunk) - 1);
            if (more > 0)
                received += more;
            //more<0 (error/disconnect) is picked up by the next blocking read

            if (stream_live || stream_flashing)
            {
                xStreamBufferSend(stream_buffer, chunk, received, portMAX_DELAY);
                stream_bytes_produced += received;
            }
        }
        ESP_LOGI(LEDSTREAMER_HTTP_TAG, "Stream ended");
    }
    else
        ESP_LOGE(LEDSTREAMER_HTTP_TAG, "Streaming failed");

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    //let the consumer play out / flash-write what's still buffered, then stop
    stream_drain();
    stream_live = false;
    stream_flashing = false;
    fileserver_close(ledstreamer_http_file_ctx);
    http_connected = false;

    //on disconnect, start replaying from flash
    ledstreamer_flash_start();
}


[[noreturn]] inline void ledstreamer_http_task(void* args)

{
    int64_t lastAttempt = 0;
    while (true)
    {
        lastAttempt = esp_timer_get_time();
        stream();
        //don't retry more than once per second
        if (esp_timer_get_time() - lastAttempt < 1000000)
        {
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
}


inline void ledstreamer_http_init()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA); // Get the MAC address

    // Format the MAC address as a string
    snprintf(url, sizeof(url), "%s/%02X%02X%02X%02X%02X%02X",
             settings_get("ledder_url"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    stream_buffer = xStreamBufferCreate(LEDSTREAMER_HTTP_BUFFER_SIZE, 1);
    assert(stream_buffer != nullptr);

    xTaskCreatePinnedToCore(ledstreamer_http_task, "ledstreamer_http_task", 4096, nullptr, 10, nullptr, 0);
    xTaskCreatePinnedToCore(ledstreamer_http_consume_task, "ledstream_decode", 4096, nullptr, 10, nullptr, 0);
}


#endif //LEDSTREA_HTTP_HPP
