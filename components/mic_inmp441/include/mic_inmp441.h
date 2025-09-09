#ifndef MIC_INMP441_H
#define MIC_INMP441_H

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

// 麦克风配置结构体
typedef struct {
    i2s_port_t i2s_port;          // I2S端口号 (I2S_NUM_0 或 I2S_NUM_1)
    uint32_t sample_rate;         // 采样率 (8000/16000/44100/48000 Hz)
    gpio_num_t lrclk_gpio;        // WS/LRCLK引脚
    gpio_num_t bclk_gpio;         // BCLK引脚
    gpio_num_t data_gpio;         // 数据输入引脚
    uint32_t dma_buf_count;       // DMA缓冲区数量 (推荐4-8)
    uint32_t dma_buf_len;         // 单个DMA缓冲区大小 (字节，推荐1024)
} mic_inmp441_config_t;

// 驱动句柄
typedef void* mic_inmp441_handle_t;

// 初始化麦克风
mic_inmp441_handle_t mic_inmp441_init(const mic_inmp441_config_t* config);

// 读取麦克风数据
int mic_inmp441_read(mic_inmp441_handle_t handle, 
                    void* data, 
                    size_t len, 
                    uint32_t timeout_ms);

// 销毁驱动
void mic_inmp441_deinit(mic_inmp441_handle_t handle);

// 创建读取任务的接口
esp_err_t mic_inmp441_create_read_task(mic_inmp441_handle_t handle, uint32_t stack_size, uint32_t priority);

#endif  // MIC_INMP441_H
