#ifndef LEDSTREAM_CONSOLE_HPP
#define LEDSTREAM_CONSOLE_HPP

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "linenoise/linenoise.h"
#include "esp_vfs.h"
#include <sys/stat.h>

#include "settings.hpp"

// Configuration console. One esp_console command table, reachable two ways:
//  - serial REPL on the normal console uart/usb (with line editing and history)
//  - tcp port 23 ("telnet", works with nc too), guarded by the console_pass setting.
//    While console_pass is empty, remote access stays disabled; set it via serial first
//    (or bake a default into the sdkconfig).
//
// Every setting in settings_defs[] is automatically a command named after its key:
// 'wifi_ssid myssid' sets it, 'wifi_ssid' alone shows it (so tab-completion works).
//
// To add another command: write an int fn(int argc, char **argv) that printf()s its
// output (stdout is redirected to the tcp client when invoked remotely) and add one
// console_register() line in console_init().

static const char *CONSOLE_TAG = "console";

#define CONSOLE_TCP_PORT 23
#define CONSOLE_PROMPT "ledstream> "
#define CONSOLE_MAX_LINE 256

//////////////////////////////////////////// commands

static void print_setting(int i) {
    const char *shown = settings_values[i];
    if (settings_defs[i].secret && strlen(shown) > 0)
        shown = "***";
    printf("%-13s = \"%s\" [%s]  %s\n", settings_defs[i].key, shown,
           settings_from_nvs[i] ? "nvs" : "default", settings_defs[i].help);
}

//shared handler for all setting commands; argv[0] is the setting's key
static int cmd_setting(int argc, char **argv) {
    int i = settings_index(argv[0]);
    if (i < 0)
        return 1;

    if (argc == 1) {
        print_setting(i);
        return 0;
    }

    char value[SETTINGS_MAX_VALUE];
    value[0] = 0;
    for (int a = 1; a < argc; a++) {
        if (a > 1)
            strlcat(value, " ", sizeof(value));
        if (strlcat(value, argv[a], sizeof(value)) >= sizeof(value)) {
            printf("value too long (max %d)\n", SETTINGS_MAX_VALUE - 1);
            return 1;
        }
    }

    if (!settings_set(argv[0], value)) {
        printf("failed to save\n");
        return 1;
    }
    printf("saved, 'reboot' to apply\n");
    return 0;
}

static int cmd_list(int argc, char **argv) {
    for (int i = 0; i < (int) SETTINGS_COUNT; i++)
        print_setting(i);
    return 0;
}

static int cmd_unset(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: unset <key>   (revert to compile-time default)\n");
        return 1;
    }
    if (!settings_erase(argv[1])) {
        printf("unknown setting '%s', try 'list'\n", argv[1]);
        return 1;
    }
    printf("reverted to default, 'reboot' to apply\n");
    return 0;
}

static int cmd_defaults(int argc, char **argv) {
    for (int i = 0; i < (int) SETTINGS_COUNT; i++)
        settings_erase(settings_defs[i].key);
    printf("all settings reverted to compile-time defaults, 'reboot' to apply\n");
    return 0;
}

static int cmd_info(int argc, char **argv) {
    const esp_app_desc_t *app = esp_ota_get_app_description();
    printf("firmware : %s (built %s %s)\n", app->version, app->date, app->time);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    printf("mac      : %02X%02X%02X%02X%02X%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK)
        printf("ip       : " IPSTR "\n", IP2STR(&ip.ip));

    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
        printf("wifi     : %s channel %d rssi %d\n", (const char *) ap.ssid, ap.primary, ap.rssi);

    printf("free heap: %lu\n", (unsigned long) esp_get_free_heap_size());
    printf("uptime   : %llds\n", esp_timer_get_time() / 1000000);
    return 0;
}

static int cmd_reboot(int argc, char **argv) {
    printf("rebooting...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_restart();
    return 0;
}

inline void console_register(const char *command, const char *help, esp_console_cmd_func_t func,
                             const char *hint = NULL) {
    esp_console_cmd_t cmd = {};
    cmd.command = command;
    cmd.help = help;
    cmd.hint = hint;
    cmd.func = func;
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

//////////////////////////////////////////// tcp ("telnet") side

//line reader on the task's stdin. filters telnet IAC negotiation so both a real
//telnet client and plain nc work; handles CR, CRLF and LF line endings.
static bool console_read_line(char *buf, int max) {
    static bool skip_lf = false;
    int len = 0;
    while (true) {
        int c = getchar();
        if (c == EOF)
            return false;

        if (skip_lf) {
            skip_lf = false;
            if (c == '\n' || c == 0)
                continue;
        }

        if (c == 0xff) { //telnet IAC
            int verb = getchar();
            if (verb == EOF)
                return false;
            if (verb == 250) { //subnegotiation: skip until IAC SE
                int prev = 0;
                while (true) {
                    int d = getchar();
                    if (d == EOF)
                        return false;
                    if (prev == 0xff && d == 240)
                        break;
                    prev = d;
                }
            } else if (verb >= 251 && verb <= 254) { //WILL/WONT/DO/DONT + option byte
                if (getchar() == EOF)
                    return false;
            }
            continue;
        }

        if (c == '\r')
            skip_lf = true;
        if (c == '\r' || c == '\n') {
            buf[len] = 0;
            return true;
        }
        if (c == 8 || c == 127) { //backspace
            if (len > 0)
                len--;
            continue;
        }
        if (c >= 32 && c < 127 && len < max - 1)
            buf[len++] = c;
    }
}

//////////////////////////////////////////// telnet char-mode (for tab completion etc)

//a real telnet client can be switched to character-at-a-time mode; then the actual
//linenoise runs over the socket and tab completion/hints/history/editing all work.
//linenoise does raw read()/write() on fileno(stdin/stdout), so the filtering
//(strip telnet IAC sequences on input, \n -> \r\n and escape 0xff on output) must
//live at fd level: a minimal VFS device "/dev/telnet" wrapping the client socket.
static struct {
    int sock;
    int state;    //0=data 1=after IAC 2=after IAC+verb 3=in subneg 4=subneg after IAC
    bool last_cr; //to swallow the LF/NUL a client sends after CR
} telnet_client;

static ssize_t telnet_vfs_read(int fd, void *dst, size_t size) {
    char *buf = (char *) dst;
    while (true) {
        int r = recv(telnet_client.sock, buf, size, 0);
        if (r <= 0)
            return r;
        int out = 0;
        for (int i = 0; i < r; i++) {
            unsigned char ch = (unsigned char) buf[i];
            switch (telnet_client.state) {
                case 0:
                    if (ch == 255) {
                        telnet_client.state = 1;
                        break;
                    }
                    //linenoise's Enter is LF; telnet sends CR LF or CR NUL
                    if (telnet_client.last_cr && (ch == '\n' || ch == 0)) {
                        telnet_client.last_cr = false;
                        break;
                    }
                    telnet_client.last_cr = (ch == '\r');
                    buf[out++] = (char) (ch == '\r' ? '\n' : ch);
                    break;
                case 1:
                    if (ch == 255) { buf[out++] = (char) ch; telnet_client.state = 0; } //escaped 0xff
                    else if (ch == 250) telnet_client.state = 3;                        //SB
                    else if (ch >= 251 && ch <= 254) telnet_client.state = 2;           //WILL/WONT/DO/DONT
                    else telnet_client.state = 0;
                    break;
                case 2: //option byte
                    telnet_client.state = 0;
                    break;
                case 3: //subnegotiation payload
                    if (ch == 255) telnet_client.state = 4;
                    break;
                case 4:
                    telnet_client.state = (ch == 240) ? 0 : 3; //SE ends it
                    break;
            }
        }
        if (out > 0)
            return out;
    }
}

static ssize_t telnet_vfs_write(int fd, const void *data, size_t size) {
    const char *buf = (const char *) data;
    char chunk[128];
    int n = 0;
    for (size_t i = 0; i < size; i++) {
        if (n >= (int) sizeof(chunk) - 2) {
            if (send(telnet_client.sock, chunk, n, 0) < 0)
                return -1;
            n = 0;
        }
        if (buf[i] == '\n')
            chunk[n++] = '\r';
        else if ((unsigned char) buf[i] == 255)
            chunk[n++] = (char) 255;
        chunk[n++] = buf[i];
    }
    if (n > 0 && send(telnet_client.sock, chunk, n, 0) < 0)
        return -1;
    return size;
}

static int telnet_vfs_open(const char *path, int flags, int mode) { return 0; }
static int telnet_vfs_close(int fd) { return 0; } //socket is closed by the tcp task
static int telnet_vfs_fstat(int fd, struct stat *st) {
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    return 0;
}
static int telnet_vfs_fcntl(int fd, int cmd, int arg) { return 0; }
static int telnet_vfs_fsync(int fd) { return 0; }

static void telnet_vfs_register() {
    esp_vfs_t vfs = {};
    vfs.flags = ESP_VFS_FLAG_DEFAULT;
    vfs.read = &telnet_vfs_read;
    vfs.write = &telnet_vfs_write;
    vfs.open = &telnet_vfs_open;
    vfs.close = &telnet_vfs_close;
    vfs.fstat = &telnet_vfs_fstat;
    vfs.fcntl = &telnet_vfs_fcntl;
    vfs.fsync = &telnet_vfs_fsync;
    ESP_ERROR_CHECK(esp_vfs_register("/dev/telnet", &vfs, NULL));
}

//offer char mode (we take over echo). a real telnet client answers with IAC
//responses within a moment; nc/raw tcp clients stay silent and get line mode.
static bool telnet_negotiate(int sock) {
    const uint8_t offer[] = {255, 251, 1,   //IAC WILL ECHO
                             255, 251, 3};  //IAC WILL SUPPRESS-GO-AHEAD
    send(sock, offer, sizeof(offer), 0);

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    struct timeval tv = {.tv_sec = 0, .tv_usec = 300000};
    if (select(sock + 1, &rfds, NULL, NULL, &tv) <= 0)
        return false;
    uint8_t c;
    //peek only: the responses themselves are consumed by telnet_read's filter
    return recv(sock, &c, 1, MSG_PEEK) == 1 && c == 255;
}

static bool console_login() {
    const char *pass = settings_get("console_pass");
    if (strlen(pass) == 0) {
        printf("remote console disabled: set console_pass via the serial console first\n");
        return false;
    }

    char line[SETTINGS_MAX_VALUE];
    for (int tries = 0; tries < 3; tries++) {
        printf("password: ");
        fflush(stdout);
        if (!console_read_line(line, sizeof(line)))
            return false;
        if (strcmp(line, pass) == 0)
            return true;
        printf("wrong password\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); //slow down guessing
    }
    return false;
}

static void console_run_line(const char *line) {
    int ret;
    esp_err_t err = esp_console_run(line, &ret);
    if (err == ESP_ERR_NOT_FOUND)
        printf("unknown command, try 'help'\n");
    else if (err != ESP_OK && err != ESP_ERR_INVALID_ARG)
        printf("error: %s\n", esp_err_to_name(err));
}

static void console_session(bool use_linenoise) {
    printf("ledstream remote console, 'help' for commands, 'exit' to disconnect\n");
    char linebuf[CONSOLE_MAX_LINE];
    while (true) {
        const char *line;
        char *edited = NULL;
        if (use_linenoise) {
            edited = linenoise(CONSOLE_PROMPT);
            if (!edited) {
                if (feof(stdin) || ferror(stdin))
                    return; //client disconnected
                //ctrl-c etc — but also any unexpected error linenoise swallows:
                //the delay keeps a retry from ever becoming a watchdog-starving spin
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
            line = edited;
        } else {
            printf(CONSOLE_PROMPT);
            fflush(stdout);
            if (!console_read_line(linebuf, sizeof(linebuf)))
                return;
            line = linebuf;
        }

        bool quit = (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0);
        if (!quit && strlen(line) > 0) {
            if (use_linenoise)
                linenoiseHistoryAdd(line);
            console_run_line(line);
        }
        if (edited)
            linenoiseFree(edited);
        if (quit)
            return;
    }
}

[[noreturn]] static void console_tcp_task(void *args) {
    //remember the task's normal stdio (uart), to restore between clients so
    //ESP_LOG from this task never hits a closed socket FILE
    FILE *orig_in = stdin, *orig_out = stdout, *orig_err = stderr;

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(CONSOLE_TCP_PORT);
    ESP_ERROR_CHECK(bind(listen_sock, (struct sockaddr *) &addr, sizeof(addr)) == 0 ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(listen(listen_sock, 1) == 0 ? ESP_OK : ESP_FAIL);
    ESP_LOGI(CONSOLE_TAG, "remote console listening on port %d", CONSOLE_TCP_PORT);

    while (true) {
        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);
        int sock = accept(listen_sock, (struct sockaddr *) &client, &client_len);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(CONSOLE_TAG, "client connected from %s", inet_ntoa(client.sin_addr));

        //drop idle/half-dead clients so the single connection slot frees up again
        struct timeval tv = {.tv_sec = 300, .tv_usec = 0};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        //char mode (real telnet client) gets the full linenoise; nc gets line mode
        bool telnet_mode = telnet_negotiate(sock);
        telnet_client.sock = sock;
        telnet_client.state = 0;
        telnet_client.last_cr = false;

        //stdio is per-task in esp-idf: pointing this task's stdin/stdout at the
        //client makes getchar()/linenoise and every command's printf() talk to it.
        //the /dev/telnet vfs device gives linenoise the real fd it needs for its
        //raw read()/write() calls (lwip socket fds work directly for the nc case).
        FILE *io = telnet_mode ? fopen("/dev/telnet", "r+")
                               : fdopen(sock, "r+");
        if (!io) {
            close(sock);
            continue;
        }
        setvbuf(io, NULL, _IONBF, 0);
        stdin = stdout = stderr = io;

        if (console_login())
            console_session(telnet_mode);
        printf("bye\n");

        fclose(io);
        if (telnet_mode)
            close(sock); //fdopen's fclose closes the socket itself, /dev/telnet's doesn't
        stdin = orig_in;
        stdout = orig_out;
        stderr = orig_err;
        ESP_LOGI(CONSOLE_TAG, "client disconnected");
    }
}

//////////////////////////////////////////// init

inline void console_init() {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = CONSOLE_PROMPT;
    repl_config.max_cmdline_length = CONSOLE_MAX_LINE;

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#endif

    //every setting is a command: '<key>' shows it, '<key> <value>' sets it
    for (int i = 0; i < (int) SETTINGS_COUNT; i++)
        console_register(settings_defs[i].key, settings_defs[i].help, &cmd_setting, "[value]");

    console_register("list", "list all settings", &cmd_list);
    console_register("unset", "revert a setting to its compile-time default", &cmd_unset, "<key>");
    console_register("defaults", "revert all settings to compile-time defaults", &cmd_defaults);
    console_register("info", "show firmware/network/system info", &cmd_info);
    console_register("reboot", "restart the device", &cmd_reboot);

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    //the repl sets these too, but the telnet sessions need them regardless
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *) &esp_console_get_hint);

    telnet_vfs_register();
    xTaskCreate(console_tcp_task, "console_tcp", 8192, nullptr, 1, nullptr);
}

#endif //LEDSTREAM_CONSOLE_HPP
