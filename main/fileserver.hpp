//
// Created by psy on 4/8/23.
//
// curl -X POST --data-binary @test.qois 192.168.13.137/upload -v

#ifndef LEDSTREAM_FILESERVER_HPP
#define LEDSTREAM_FILESERVER_HPP

#include "esp_partition.h"

#include "esp_http_client.h"

const char *FILESERVER_TAG = "fileserver";


//flash stuff
#define FILESERVER_FILEINFO_OFFSET           (SPI_FLASH_SEC_SIZE*0)
#define FILESERVER_ANIMATION_OFFSET          (SPI_FLASH_SEC_SIZE*1)
int fileserver_max_file_size=0;

const esp_partition_t *fileserver_partition;
char fileserver_buffer[SPI_FLASH_SEC_SIZE];
size_t fileserver_buffer_offset=0;

size_t fileserver_read_offset;

bool fileserver_downloading;

struct file_info_t {
    size_t len;

} fileserver_current_file;



//load fileserver_current_file from flash
inline esp_err_t fileserver_load_file_info() {
    if (esp_partition_read_raw(fileserver_partition, FILESERVER_FILEINFO_OFFSET, &fileserver_current_file,
                               sizeof(fileserver_current_file)) !=
        ESP_OK) {
        ESP_LOGE(FILESERVER_TAG, "error while reading file info");
        return ESP_FAIL;
    }
    return ESP_OK;

}

//store fileserver_current_file to flash
inline esp_err_t fileserver_save_file_info() {
    esp_partition_erase_range(fileserver_partition, FILESERVER_FILEINFO_OFFSET, SPI_FLASH_SEC_SIZE);
    if (esp_partition_write_raw(fileserver_partition, FILESERVER_FILEINFO_OFFSET, &fileserver_current_file,
                                sizeof(fileserver_current_file)) !=
        ESP_OK) {
        ESP_LOGE(FILESERVER_TAG, "error while writing file info");
        return ESP_FAIL;
    }
    return ESP_OK;
}


//prepare new download
inline void fileserver_prepare_download()
{
    ESP_LOGI(FILESERVER_TAG, "Preparing flash for download of %s", fileserver_download_url);

    //store timestamp to prevent download loops in case of crash.
    fileserver_current_file.len = 0;
    fileserver_save_file_info();

    fileserver_downloading=true;
    esp_partition_erase_range(fileserver_partition, FILESERVER_ANIMATION_OFFSET,fileserver_max_file_size );

}



inline void fileserver_write(const void *buf, size_t len)
{


                        if (esp_partition_write_raw(fileserver_partition, write_offset, evt->data, evt->data_len) !=
                            ESP_OK) {
                            ESP_LOGE(FILESERVER_TAG, "flash write error at offset %d", write_offset);

}


inline esp_err_t fileserver_init() {



                            fileserver_current_file.len = 0;
    fileserver_current_file.timestamp = 0;
    fileserver_downloading=false;


    fileserver_partition = (esp_partition_t *) esp_partition_find_first(ESP_PARTITION_TYPE_ANY,
                                                                        ESP_PARTITION_SUBTYPE_ANY, "ledstream");
    if (fileserver_partition == nullptr) {
        ESP_LOGE(FILESERVER_TAG, "Cant find ledstream partition");
        return ESP_FAIL;
    }


    fileserver_max_file_size=fileserver_partition->size - FILESERVER_ANIMATION_OFFSET;


    fileserver_load_file_info();


    char mac_str[18];
    get_mac_address(mac_str);

    snprintf(fileserver_download_url, sizeof(fileserver_download_url), "%s/get/render/%s", CONFIG_LEDSTREAM_LEDDER_URL,
             mac_str);
    snprintf(fileserver_status_url, sizeof(fileserver_download_url), "%s/get/status/%s", CONFIG_LEDSTREAM_LEDDER_URL,
             mac_str);

    xTaskCreate(fileserver_task, "fileserver_task", 4096, nullptr, 1, nullptr);


    return ESP_OK;

}



//read next byte of file
inline uint8_t fileserver_read_next() {
    auto blockOffset = fileserver_read_offset % SPI_FLASH_SEC_SIZE;



    //arrived at new block ,cache it
    if (blockOffset == 0) {
        if (esp_partition_read_raw(fileserver_partition, FILESERVER_ANIMATION_OFFSET + fileserver_read_offset,
                                   fileserver_buffer,
                                   SPI_FLASH_SEC_SIZE) == ESP_FAIL) {
            ESP_LOGE(FILESERVER_TAG, "error while reading offset %d", fileserver_read_offset);
            fileserver_read_restart();
            return 0;
        }

    }

    //increase offset and loop
    fileserver_read_offset++;
    if (fileserver_read_offset >= fileserver_current_file.len)
        fileserver_read_restart();

    return fileserver_buffer[blockOffset];

}


#endif //LEDSTREAM_FILESERVER_HPP
