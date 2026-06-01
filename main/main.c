#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "driver/i2s_std.h"
#include "wifi_creds.h"

// --- Configuration ---
#define UDP_IP    "192.168.6.48" // IP of your computer
#define UDP_PORT  8888
#define SAMPLES_PER_READ 240

// --- WiFi Event Handler ---
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) esp_wifi_connect();
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

    // Setup I2S (Keep your working settings here)
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &rx_handle);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(22050),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = { .bclk = 33, .ws = 25, .din = 32, .dout = I2S_GPIO_UNUSED, .mclk = I2S_GPIO_UNUSED,
                      .invert_flags = { .bclk_inv = true, .ws_inv = false } },
    };
    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);

    // Setup UDP Socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in dest_addr = { .sin_family = AF_INET, .sin_port = htons(UDP_PORT), .sin_addr.s_addr = inet_addr(UDP_IP) };

    int32_t raw_buffer[SAMPLES_PER_READ]; // Holds 64 samples (32 stereo frames)
    uint8_t packed_buffer[SAMPLES_PER_READ * 3]; // 32 frames * 2 channels * 3 bytes = 192 bytes
    size_t r_bytes = 0;

    while (1) {
        if (i2s_channel_read(rx_handle, raw_buffer, sizeof(raw_buffer), &r_bytes, 1000) == ESP_OK) {
            size_t num_samples = r_bytes / sizeof(int32_t); // Should be 64
            size_t packed_idx = 0;

            for (size_t i = 0; i < num_samples; i++) {
                // 1. Apply your working shift and mask logic
                int32_t clean_sample = (raw_buffer[i] >> 8) & 0xFFFFFF;
                if (clean_sample & 0x800000) {
                    clean_sample |= -16777216;
                }

                // 2. Pack the 32-bit int into exactly 3 bytes (Little-Endian)
                packed_buffer[packed_idx++] = (clean_sample >> 0) & 0xFF;
                packed_buffer[packed_idx++] = (clean_sample >> 8) & 0xFF;
                packed_buffer[packed_idx++] = (clean_sample >> 16) & 0xFF;
            }

            // 3. Send the tightly packed 24-bit interleaved stream
            sendto(sock, packed_buffer, packed_idx, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }
    }
}
