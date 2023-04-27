//
// Created by psy on 4/8/23.
//
// curl -X POST --data-binary @test.qois 192.168.13.137/upload -v

#ifndef LEDSTREAM_FILESERVER_HPP
#define LEDSTREAM_FILESERVER_HPP

#include "esp_http_server.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"
#include "cJSON.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "cJSON.h"

const char *FILESERVER_TAG = "fileserver";


//flash stuff
#define FILESERVER_FILEINFO_OFFSET           (SPI_FLASH_SEC_SIZE*0)
#define FILESERVER_ANIMATION_OFFSET          (SPI_FLASH_SEC_SIZE*1)

const esp_partition_t *fileserver_partition;
char fileserver_buffer[SPI_FLASH_SEC_SIZE];
size_t fileserver_read_offset;

//animation stuff
uint32_t fileserver_latest_timestamp = 0;
struct file_info_t {
    uint32_t timestamp;
    size_t len;

} fileserver_current_file;


char fileserver_download_url[100];
char fileserver_status_url[100];

//load fileserver_current_file from flash
esp_err_t fileserver_load_file_info() {
    if (esp_partition_read_raw(fileserver_partition, FILESERVER_FILEINFO_OFFSET, &fileserver_current_file,
                               sizeof(fileserver_current_file)) !=
        ESP_OK) {
        ESP_LOGE(FILESERVER_TAG, "error while reading file info");
        return ESP_FAIL;
    }
    return ESP_OK;

}

//store fileserver_current_file to flash
esp_err_t fileserver_save_file_info() {
    esp_partition_erase_range(fileserver_partition, FILESERVER_FILEINFO_OFFSET, SPI_FLASH_SEC_SIZE);
    if (esp_partition_write_raw(fileserver_partition, FILESERVER_FILEINFO_OFFSET, &fileserver_current_file,
                                sizeof(fileserver_current_file)) !=
        ESP_OK) {
        ESP_LOGE(FILESERVER_TAG, "error while reading file info");
        return ESP_FAIL;
    }
    return ESP_OK;
}

//check online for latest update timestamp
void fileserver_get_online_timestamp() {

    ESP_LOGI(FILESERVER_TAG, "Checking latest animation version at: %s", fileserver_status_url);

    esp_http_client_config_t config = {
            .url = fileserver_status_url,
            .event_handler=[](esp_http_client_event_t *evt) {
                switch (evt->event_id) {
                    default:
                        break;
                    case HTTP_EVENT_ON_DATA:
                        cJSON *root = cJSON_Parse((char *) evt->data);
                        if (root != nullptr) {
                            cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
                            if (timestamp != nullptr) {
                                fileserver_latest_timestamp = timestamp->valueint;
                                ESP_LOGI(FILESERVER_TAG, "Latest version: %d", fileserver_latest_timestamp);
                            }
                            cJSON_Delete(root);
                        }

                        break;
                }
                return ESP_OK;
            }
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);

}

void fileserver_download() {

    //no download needed?
    if (fileserver_latest_timestamp == 0 || fileserver_current_file.timestamp == fileserver_latest_timestamp)
        return;

    ESP_LOGI(FILESERVER_TAG, "Preparing flash for download of %s", fileserver_download_url);
    esp_partition_erase_range(fileserver_partition, FILESERVER_ANIMATION_OFFSET,
                              fileserver_partition->size - FILESERVER_ANIMATION_OFFSET);

    ESP_LOGI(FILESERVER_TAG, "Downloading....");
    fileserver_current_file.len = 0;


    esp_http_client_config_t config = {
            .url = fileserver_download_url,
            .event_handler=[](esp_http_client_event_t *evt) {
                switch (evt->event_id) {
                    default:
                        break;
                    case HTTP_EVENT_ON_DATA:

                        auto write_offset = fileserver_current_file.len + FILESERVER_ANIMATION_OFFSET;
                        if (esp_partition_write_raw(fileserver_partition, write_offset, evt->data, evt->data_len) !=
                            ESP_OK) {
                            ESP_LOGE(FILESERVER_TAG, "flash write error at offset %d", write_offset);
                            return ESP_FAIL;
                        }

                        fileserver_current_file.len += evt->data_len;

                        break;
                }
                return ESP_OK;
            }
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (esp_http_client_perform(client) == ESP_OK) {
        //we did it, store new timestamp and write it to flash
        ESP_LOGI(FILESERVER_TAG, "Download succes");
        fileserver_current_file.timestamp = fileserver_latest_timestamp;
        fileserver_save_file_info();
    }
    esp_http_client_cleanup(client);


}

[[noreturn]]  void fileserver_task(void *args) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        fileserver_get_online_timestamp();
        fileserver_download();

    }
}

esp_err_t fileserver_start() {

    fileserver_current_file.len = 0;
    fileserver_current_file.timestamp = 0;


    fileserver_partition = (esp_partition_t *) esp_partition_find_first(ESP_PARTITION_TYPE_ANY,
                                                                        ESP_PARTITION_SUBTYPE_ANY, "ledstream");
    if (fileserver_partition == nullptr) {
        ESP_LOGE(FILESERVER_TAG, "Cant find ledstream partition");
        return ESP_FAIL;
    }

    fileserver_load_file_info();


    char mac_str[18];
    get_mac_address(mac_str);

    snprintf(fileserver_download_url, sizeof(fileserver_download_url), "%s/get/file/%s", CONFIG_LEDSTREAM_LEDDER_URL,
             mac_str);
    snprintf(fileserver_status_url, sizeof(fileserver_download_url), "%s/get/status/%s", CONFIG_LEDSTREAM_LEDDER_URL,
             mac_str);

    xTaskCreate(fileserver_task, "fileserver_task", 4096, nullptr, 1, nullptr);


    return ESP_OK;

}

esp_err_t fileserver_read_start() {
    fileserver_read_offset = 0;
    return ESP_OK;
}

uint8_t fileserver_read_next() {
    auto blockOffset = fileserver_read_offset % SPI_FLASH_SEC_SIZE;

    //arrived at new block ,cache it
    if (blockOffset == 0) {
//            ESP_LOGI(FILESERVER_TAG, "reading %d bytes at offset %d", SPI_FLASH_SEC_SIZE, fileserver_read_offset);
        if (esp_partition_read_raw(fileserver_partition, fileserver_read_offset, fileserver_buffer,
                                   SPI_FLASH_SEC_SIZE) == ESP_FAIL) {
            ESP_LOGE(FILESERVER_TAG, "error while reading offset %d", fileserver_read_offset);
        }
//            ESP_LOGI(FILESERVER_TAG, "reading completed");

    }

    fileserver_read_offset++;

    return fileserver_buffer[blockOffset];

}


#endif //LEDSTREAM_FILESERVER_HPP
