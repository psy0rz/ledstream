// host-side stub of the esp glue used by the firmware sources
#pragma once
#include <cstdio>

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#define ESP_LOGD(tag, ...) ((void)0)
#define ESP_LOGI(tag, ...) ((void)0)
#define ESP_LOGW(tag, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E %s: " fmt "\n", tag, ##__VA_ARGS__)
