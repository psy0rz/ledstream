//
// Created by psy on 4/8/23.
//
// curl -X POST --data-binary @test.qois 192.168.13.137/upload -v

#ifndef LEDSTREAM_FILESERVER_HPP
#define LEDSTREAM_FILESERVER_HPP

#include "esp_http_server.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"


static const esp_partition_t *partition;
static char buffer[SPI_FLASH_SEC_SIZE];
static size_t readOffset;

static bool writing=false;

class FileServer {

private:
    static const char *TAG;

public:


    static esp_err_t post_handler(httpd_req_t *req) {

        size_t total_size = req->content_len;
        size_t write_offset = 0;
        writing=true;

        ESP_LOGI(TAG, "erasing flash for uppload of  %d bytes..", total_size);
        const size_t aligned_size = ((total_size / SPI_FLASH_SEC_SIZE) + 1) * SPI_FLASH_SEC_SIZE;
        esp_partition_erase_range(partition, 0, aligned_size);

        ESP_LOGI(TAG, "receiving data..");

        while (write_offset < total_size) {
            int bytes_received = httpd_req_recv(req, buffer, SPI_FLASH_SEC_SIZE);
            if (bytes_received <= 0) {
                if (bytes_received == HTTPD_SOCK_ERR_TIMEOUT) {
                    /* Retry if timeout occurred */
                    continue;
                }
                ESP_LOGE(TAG, "File reception failed!");
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
                return ESP_FAIL;
            }

//            ESP_LOGI(TAG, "writing %d", bytes_received);
            if (esp_partition_write_raw(partition, write_offset, buffer, bytes_received) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write to partition");
                ESP_LOGE(TAG, "write error");
                return ESP_FAIL;
            }
            write_offset = write_offset + bytes_received;
        }

        if (write_offset != total_size) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive complete data");
            ESP_LOGE(TAG, "incomplete");
            return ESP_FAIL;
        }

        httpd_resp_send(req, nullptr, 0);
        ESP_LOGI(TAG, "recieve ok");

        return ESP_OK;
    }

    static esp_err_t startServer() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        httpd_handle_t server;


        partition = (esp_partition_t *) esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY,
                                                                 "ledstream");
        if (partition == nullptr) {
            ESP_LOGE(TAG, "Cant find ledstream partition");
            return ESP_FAIL;
        }

        if (httpd_start(&server, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start webserver");
            return ESP_FAIL;
        }

        httpd_uri_t post_uri;
        post_uri.uri = "/upload";
        post_uri.method = HTTP_POST;
        post_uri.handler = post_handler;
        post_uri.user_ctx = nullptr;

        httpd_register_uri_handler(server, &post_uri);

        ESP_LOGI(TAG, "Started webserver");
        return ESP_OK;

    }

    static esp_err_t readStart() {
        readOffset = 0;
        return ESP_OK;
    }

    static uint8_t readNext() {
        auto blockOffset = readOffset % SPI_FLASH_SEC_SIZE;

        //arrived at new block ,cache it
        if (blockOffset == 0) {
//            ESP_LOGI(TAG, "reading %d bytes at offset %d", SPI_FLASH_SEC_SIZE, readOffset);
            if (esp_partition_read_raw(partition, readOffset, buffer, SPI_FLASH_SEC_SIZE) == ESP_FAIL) {
                ESP_LOGE(TAG, "error while reading offset %d", readOffset);
            }
//            ESP_LOGI(TAG, "reading completed");

        }

        readOffset++;

        return buffer[blockOffset];

    }


};

const char *FileServer::TAG = "fileserver";

#endif //LEDSTREAM_FILESERVER_HPP
