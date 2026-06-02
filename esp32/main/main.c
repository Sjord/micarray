#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "driver/i2s_std.h"
#include "wifi_creds.h"
#include "qoa.h"

// --- Configuration ---
#define UDP_IP    "192.168.6.48" // IP of your computer
#define UDP_PORT  8888
#define SAMPLES_PER_READ 240
#define I2S_CHANNEL_COUNT 2

int32_t raw_buffer[I2S_CHANNEL_COUNT][SAMPLES_PER_READ];
int16_t samples_buffer[I2S_CHANNEL_COUNT * SAMPLES_PER_READ];
uint8_t packed_buffer[SAMPLES_PER_READ * 3 * I2S_CHANNEL_COUNT];

// --- WiFi Event Handler ---
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) esp_wifi_connect();
}

i2s_chan_handle_t setup_i2s(int port, int pin, i2s_role_t role) {
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, role);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(22050),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { .bclk = 33, .ws = 25, .din = pin, .dout = I2S_GPIO_UNUSED, .mclk = I2S_GPIO_UNUSED,
                      .invert_flags = { .bclk_inv = true, .ws_inv = false } },
    };
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
    return rx_handle;
}

void app_main(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS }};
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    i2s_chan_handle_t rx_handles[I2S_CHANNEL_COUNT] = {
        setup_i2s(I2S_NUM_0, 32, I2S_ROLE_SLAVE),
        setup_i2s(I2S_NUM_1, 26, I2S_ROLE_MASTER)
    };

    // Setup UDP Socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in dest_addr = { .sin_family = AF_INET, .sin_port = htons(UDP_PORT), .sin_addr.s_addr = inet_addr(UDP_IP) };

    size_t r_bytes = 0;

    qoa_desc qoa;
    memset(&qoa, 0, sizeof(qoa_desc));
    qoa.channels = 4;
    qoa.samplerate = 22050;

    while (1) {
        for (int ch = 0; ch < I2S_CHANNEL_COUNT; ch++) {
            esp_err_t res = i2s_channel_read(rx_handles[ch], raw_buffer[ch], sizeof(raw_buffer[ch]), &r_bytes, 1000);
            if (res != ESP_OK || r_bytes != sizeof(raw_buffer[ch])) {
                // Error
                memset(raw_buffer[ch], 0, sizeof(raw_buffer[ch]));
            }
        }

        int16_t * sample_pointer = samples_buffer;
        for (size_t i = 0; i < SAMPLES_PER_READ; i++) {
            for (int ch = 0; ch < I2S_CHANNEL_COUNT; ch++) {
                int16_t clean = (raw_buffer[ch][i] >> 16) & 0xFFFF;
                *sample_pointer = clean;
                sample_pointer++;
            }
        }

        size_t bytes_written = qoa_encode_frame(
            samples_buffer,
            &qoa,
            SAMPLES_PER_READ / 2,
            packed_buffer
        );

        // Send the single 1440-byte 4-channel network packet
        sendto(sock, packed_buffer, bytes_written, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    }
}
