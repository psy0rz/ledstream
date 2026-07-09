#include <esp_littlefs.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"

#ifndef FILESERVER_H
#define FILESERVER_H

#define FLASH_BLOCK_SIZE 4096  // Must match physical flash page size
#define STREAM_FILE "/littlefs/stream.bin"
#define FLASH_PARTITION_LABEL "ledstream"
//littlefs needs free blocks for its copy-on-write metadata updates; filling
//the filesystem to the last byte corrupts it, so stop recording well before
#define FLASH_RESERVE (16 * FLASH_BLOCK_SIZE)


const char* FILESERVER_TAG = "fileserver";

typedef struct
{
    FILE* file;
    uint8_t* buffer; // DMA-capable aligned buffer
    size_t buffered; // Valid bytes in buffer
    bool write_mode; // Read/write mode flag
    bool read_error; // fread failed (corrupt filesystem), not just EOF
    bool write_error; // write failed or space limit reached; further writes are dropped
    size_t write_limit; // max bytes to write (free space minus reserve)
    size_t written; // bytes written to file so far
} fileserver_ctx;

// Allocate aligned buffer (critical for SPI DMA)
static uint8_t* alloc_aligned_buffer()
{
    return static_cast<uint8_t*>(heap_caps_aligned_alloc(64, FLASH_BLOCK_SIZE, MALLOC_CAP_DMA));
}

// Open stream in specified mode
inline fileserver_ctx* fileserver_open(bool write)
{
    // ESP_LOGI(FILESERVER_TAG, "opening %d", write);

    fileserver_ctx* ctx = static_cast<fileserver_ctx*>(calloc(1, sizeof(fileserver_ctx)));

    if (!ctx)
    {
        ESP_LOGE(FILESERVER_TAG, "failed to allocate memory for fileserver_ctx");
        return nullptr;
    }

    ctx->buffer = alloc_aligned_buffer();
    if (!ctx->buffer)
    {
        ESP_LOGE(FILESERVER_TAG, "failed to allocate buffer for ctx->buffer");
        free(ctx);
        return nullptr;
    }

    ctx->write_mode = write;
    ctx->file = fopen(STREAM_FILE, write ? "wb" : "rb");

    if (!ctx->file)
    {
        ESP_LOGE(FILESERVER_TAG, "failed to open file %s", STREAM_FILE);
        free(ctx->buffer);
        free(ctx);
        return nullptr;
    }

    if (write)
    {
        //determine how much we may write: littlefs corrupts when filled to the
        //brim, so keep FLASH_RESERVE free. (query after fopen: "wb" truncated
        //the previous recording, freeing its blocks)
        size_t total = 0, used = 0;
        if (esp_littlefs_info(FLASH_PARTITION_LABEL, &total, &used) == ESP_OK &&
            total > used + FLASH_RESERVE)
        {
            ctx->write_limit = total - used - FLASH_RESERVE;
            ESP_LOGI(FILESERVER_TAG, "recording, max size %u bytes", (unsigned)ctx->write_limit);
        }
        else
        {
            ESP_LOGE(FILESERVER_TAG, "no free space to record");
            ctx->write_error = true;
        }
    }

    return ctx;
}

// High-performance write with block alignment
inline void fileserver_write(fileserver_ctx* ctx, const void* data, size_t size)
{
    if (ctx == nullptr || ctx->write_error)
        return;

    const uint8_t* src = static_cast<const uint8_t*>(data);
    while (size > 0)
    {
        const size_t free_space = FLASH_BLOCK_SIZE - ctx->buffered;
        const size_t copy_size = (size > free_space) ? free_space : size;

        memcpy(ctx->buffer + ctx->buffered, src, copy_size);
        ctx->buffered += copy_size;
        src += copy_size;
        size -= copy_size;

        if (ctx->buffered == FLASH_BLOCK_SIZE)
        {
            if (ctx->written + FLASH_BLOCK_SIZE > ctx->write_limit)
            {
                ESP_LOGW(FILESERVER_TAG, "disk full, stopping recording at %u bytes", (unsigned)ctx->written);
                ctx->write_error = true;
                ctx->buffered = 0;
                return;
            }
            ESP_LOGD(FILESERVER_TAG, "writing block");
            if (fwrite(ctx->buffer, 1, FLASH_BLOCK_SIZE, ctx->file) != FLASH_BLOCK_SIZE)
            {
                ESP_LOGE(FILESERVER_TAG, "write error on %s, disk full?", STREAM_FILE);
                ctx->write_error = true;
                ctx->buffered = 0;
                return;
            }
            ctx->written += FLASH_BLOCK_SIZE;
            ctx->buffered = 0;
        }
    }
}

// read and fill buffer in ctx, ret false when done
inline bool fileserver_read(fileserver_ctx* ctx)
{
    if (ctx == nullptr)
        return false;

    // int64_t s=esp_timer_get_time();


    ctx->buffered = fread(ctx->buffer, 1, FLASH_BLOCK_SIZE, ctx->file);
    // ESP_LOGE(FILESERVER_TAG, "read lag uS: %lld", (long long)(esp_timer_get_time()-s));

    if (ctx->buffered < FLASH_BLOCK_SIZE && ferror(ctx->file))
    {
        ESP_LOGE(FILESERVER_TAG, "read error on %s, filesystem corrupt?", STREAM_FILE);
        ctx->read_error = true;
        return false;
    }

    //a recording usually ends on a partial block: still return it, the caller
    //gets false on the next call when fread returns 0
    return (ctx->buffered > 0);
}

// Close stream and free resources
inline void fileserver_close(fileserver_ctx* & ctx)
{
    if (ctx)
    {
        // ESP_LOGI(FILESERVER_TAG, "closing");
        if (ctx->write_mode && ctx->buffered > 0 && !ctx->write_error &&
            ctx->written + ctx->buffered <= ctx->write_limit)
        {
            if (fwrite(ctx->buffer, 1, ctx->buffered, ctx->file) != ctx->buffered)
            {
                ESP_LOGE(FILESERVER_TAG, "write error on %s, disk full?", STREAM_FILE);
                ctx->write_error = true;
            }
        }
        //always fclose, even after a write error: the file must not leak and
        //what was written so far stays a valid (truncated) recording
        if (fclose(ctx->file) != 0)
            ESP_LOGE(FILESERVER_TAG, "close error on %s, disk full?", STREAM_FILE);
        free(ctx->buffer);
        free(ctx);
        ctx = nullptr;
    }
}

// Usage example:
inline void fileserver_init()
{
    // Initialize LittleFS with max performance settings
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "ledstream",
        .partition = NULL,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false,
    };
    esp_vfs_littlefs_register(&conf);
}

#endif
