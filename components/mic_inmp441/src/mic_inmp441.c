#include "mic_inmp441.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include <stdlib.h>

static const char* TAG = "mic_inmp441";

// 内部设备结构体
typedef struct {
    mic_inmp441_config_t config;
    i2s_chan_handle_t rx_chan;   // I2S接收通道句柄
    bool is_init;                // 初始化状态标记
} mic_dev_t;

mic_inmp441_handle_t mic_inmp441_init(const mic_inmp441_config_t* config) {
    if (!config) {
        ESP_LOGE(TAG, "配置参数不能为空");
        return NULL;
    }

    // 检查GPIO有效性
    if (config->lrclk_gpio == GPIO_NUM_NC || config->bclk_gpio == GPIO_NUM_NC || 
        config->data_gpio == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "GPIO配置无效");
        return NULL;
    }

    // 检查采样率合理性
    if (config->sample_rate == 0) {
        ESP_LOGE(TAG, "采样率不能为0");
        return NULL;
    }

    // 分配设备内存
    mic_dev_t* dev = (mic_dev_t*)calloc(1, sizeof(mic_dev_t));
    if (!dev) {
        ESP_LOGE(TAG, "内存分配失败");
        return NULL;
    }
    dev->config = *config;
    dev->is_init = false;

    // 配置I2S标准模式参数
    i2s_std_config_t i2s_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config->sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,  // INMP441不需要主时钟
            .bclk = config->bclk_gpio,
            .ws = config->lrclk_gpio,
            .dout = I2S_GPIO_UNUSED,  // 只接收不发送
            .din = config->data_gpio,
            .invert_flags = {
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    // 创建I2S接收通道
    i2s_chan_config_t chan_cfg = {
        .id = config->i2s_port,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = config->dma_buf_count,
        .dma_frame_num = config->dma_buf_len / 4,  // 32位数据，每个frame占4字节
    };
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &dev->rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "创建I2S通道失败: %s", esp_err_to_name(ret));
        free(dev);
        return NULL;
    }

    // 初始化I2S接收通道
    ret = i2s_channel_init_std_mode(dev->rx_chan, &i2s_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "初始化I2S模式失败: %s", esp_err_to_name(ret));
        i2s_del_channel(dev->rx_chan);
        free(dev);
        return NULL;
    }

    // 启动接收
    ret = i2s_channel_enable(dev->rx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "启动I2S接收失败: %s", esp_err_to_name(ret));
        i2s_del_channel(dev->rx_chan);
        free(dev);
        return NULL;
    }

    dev->is_init = true;
    ESP_LOGI(TAG, "INMP441初始化成功 (端口: %d, 采样率: %lu Hz)", 
             config->i2s_port, config->sample_rate);
    return (mic_inmp441_handle_t)dev;
}

int mic_inmp441_read(mic_inmp441_handle_t handle, void* data, size_t len, uint32_t timeout_ms) {
    if (!handle || !data || len == 0) {
        ESP_LOGE(TAG, "无效参数");
        return -1;
    }

    mic_dev_t* dev = (mic_dev_t*)handle;
    if (!dev->is_init) {
        ESP_LOGE(TAG, "设备未初始化");
        return -1;
    }

    size_t bytes_read = 0;
    esp_err_t ret = i2s_channel_read(dev->rx_chan, data, len, &bytes_read, 
                                    pdMS_TO_TICKS(timeout_ms));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读取失败: %s", esp_err_to_name(ret));
        return -1;
    }

    // INMP441输出24位数据，存储在32位的高24位，右移8位对齐
    int32_t* samples = (int32_t*)data;
    for (size_t i = 0; i < bytes_read / sizeof(int32_t); i++) {
        samples[i] >>= 8;
    }

    return bytes_read;
}

void mic_inmp441_deinit(mic_inmp441_handle_t handle) {
    if (handle) {
        mic_dev_t* dev = (mic_dev_t*)handle;
        if (dev->is_init) {
            i2s_channel_disable(dev->rx_chan);
            i2s_del_channel(dev->rx_chan);
            dev->is_init = false;
        }
        free(dev);
        ESP_LOGI(TAG, "INMP441驱动已销毁");
    }
}

// 库内部定义任务函数
static void mic_inmp441_read_task(void *arg) {
    mic_inmp441_handle_t handle = (mic_inmp441_handle_t)arg;
    if (!handle) {
        ESP_LOGE(TAG, "无效的设备句柄");
        vTaskDelete(NULL);
    }

    mic_dev_t *dev = (mic_dev_t *)handle;
    if (!dev->is_init) {
        ESP_LOGE(TAG, "设备未初始化");
        vTaskDelete(NULL);
    }

    int32_t sample_buf[256];  // 32位采样缓冲区

    while (1) {
        // 读取数据（超时100ms）
        int bytes_read = mic_inmp441_read(
            handle,
            sample_buf,
            sizeof(sample_buf),
            100
        );

        if (bytes_read > 0) {
            size_t sample_count = bytes_read / sizeof(int32_t);
            ESP_LOGI(TAG, "读取到 %u 个采样（示例值: %ld）", 
                     sample_count, sample_buf[0]);
        } else if (bytes_read < 0) {
            ESP_LOGE(TAG, "读取数据失败");
        }

        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

esp_err_t mic_inmp441_create_read_task(mic_inmp441_handle_t handle, uint32_t stack_size, uint32_t priority) {
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mic_dev_t *dev = (mic_dev_t *)handle;
    if (!dev->is_init) {
        return ESP_ERR_INVALID_STATE;
    }

    // 创建任务
    if (xTaskCreate(
        mic_inmp441_read_task,    // 任务函数
        "mic_read_task",          // 任务名称
        stack_size,               // 栈大小
        handle,                   // 传递句柄
        priority,                 // 优先级
        NULL                      // 任务句柄
    ) != pdPASS) {
        ESP_LOGE(TAG, "任务创建失败");
        return ESP_FAIL;
    }

    return ESP_OK;
}
