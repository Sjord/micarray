#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h" // Updated header
#include "esp_log.h"

static const char *TAG = "I2S_MIC";

void app_main(void) {
    // 1. I2S Standard Configuration
    i2s_chan_handle_t rx_handle;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(7800), // Change from 48000 to 7800
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = 33,
            .ws = 25,
            .dout = I2S_GPIO_UNUSED,
            .din = 32,
            .invert_flags = { .mclk_inv = false, .bclk_inv = true, .ws_inv = false },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    int32_t raw_samples[32]; // Buffer
    size_t r_bytes = 0;

    while (1) {
        // Read 2 samples (1 stereo frame: Left and Right)
        if (i2s_channel_read(rx_handle, raw_samples, sizeof(raw_samples), &r_bytes, 1000) == ESP_OK) {
            
            // Process Left Channel
            int32_t left_raw = (raw_samples[0] >> 8) & 0xFFFFFF;
            if (left_raw & 0x800000) left_raw |= -16777216;
            
            // Process Right Channel
            int32_t right_raw = (raw_samples[1] >> 8) & 0xFFFFFF;
            if (right_raw & 0x800000) right_raw |= -16777216;

            // Print them side-by-side
            printf("L: %7ld | R: %7ld\n", left_raw, right_raw);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}
