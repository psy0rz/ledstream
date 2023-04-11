//
// Created by psy on 4/8/23.
//

#ifndef LEDSTREAM_FILESERVER_HPP
#define LEDSTREAM_FILESERVER_HPP
#include "esp_http_server.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"

#define TAG "HTTP_SERVER"

class FileUploadHandler {
public:
    FileUploadHandler() {}

    static esp_err_t post_handler(httpd_req_t *req) {
        esp_err_t err = ESP_OK;
        const esp_partition_t* partition = esp_partition_find_first(0x40, 0x00, "ledder");
        if (partition == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to find partition");
            return ESP_FAIL;
        }

        uint8_t buffer[64];
        size_t size = sizeof(buffer);

        int total_size = req->content_len;
        int received_size = 0;

        while (received_size < total_size) {
            int bytes_received = httpd_req_recv(req, buffer, size);

            if (bytes_received == 0) {
                break;
            }

            received_size += bytes_received;

            if (esp_partition_write(partition, received_size - bytes_received, buffer, bytes_received) != ESP_OK) {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write to partition");
                return ESP_FAIL;
            }
        }

        if (received_size != total_size) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive complete data");
            return ESP_FAIL;
        }

        httpd_resp_send(req, NULL, 0);
        return ESP_OK;
    }

    void startServer() {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();

        httpd_handle_t server;

        if (httpd_start(&server, &config) == ESP_OK) {
            httpd_uri_t post_uri;
            post_uri.uri = "/upload";
            post_uri.method = HTTP_POST;
            post_uri.handler = post_handler;
            post_uri.user_ctx = NULL;

            httpd_register_uri_handler(server, &post_uri);
        }
    }
};

#endif //LEDSTREAM_FILESERVER_HPP
