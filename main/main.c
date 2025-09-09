#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sh1106.h"
#include "mic_inmp441.h"
#include "wifi_module.h"



void app_main(void) {

    // 配置INMP441参数
    mic_inmp441_config_t inmp441_config = {
        .i2s_port = 0,                // I2S端口0
        .sample_rate = 16000,         // 16kHz采样率
        .lrclk_gpio = 4,              // WS/LRCLK引脚
        .bclk_gpio = 5,               // SCK/BCLK引脚
        .data_gpio = 6,               // SD/DATA引脚
        .dma_buf_count = 4,           // 4个DMA缓冲区
        .dma_buf_len = 1024           // 每个缓冲区1024字节
    };

    // 初始化INMP441
    mic_inmp441_handle_t mic = mic_inmp441_init(&inmp441_config);
    if (!mic) {
        printf("mic_inmp441 initialization failed!\n");
        return;
    }
    printf("mic_inmp441 initialization successfully\n");

    // 初始化 WiFi 模块
    ESP_ERROR_CHECK(wifi_module_init());

    // // 扫描 WiFi 列表
    ESP_ERROR_CHECK(wifi_module_scan_ap());

    // 连接指定 WiFi
    ESP_ERROR_CHECK(wifi_module_connect("1111", "00000000"));

    // 等待连接成功（实际项目中可通过事件或超时判断）
    vTaskDelay(pdMS_TO_TICKS(5000));

    // 获取 IP 地址
    char ip_str[16] = {0};
    ESP_ERROR_CHECK(wifi_module_get_ip(ip_str, sizeof(ip_str)));
    printf("Device IP: %s\n", ip_str);

    // 获取信号强度
    int8_t rssi = wifi_module_get_rssi();
    printf("WiFi RSSI: %d dBm\n", rssi);

    // 后续可根据需求调用断开连接等函数
    // ESP_ERROR_CHECK(wifi_module_disconnect());

    // 【调用库接口创建读取任务】
    esp_err_t ret = mic_inmp441_create_read_task(mic, 4096, 5);

    // 配置SH1106参数
    sh1106_config_t sh1106_config = {
        .i2c_port = 0,
        .i2c_addr = 0x3C,         // 根据实际地址修改 (0x3C或0x3D)
        .clk_speed = 100000,      // 100kHz时钟频率，兼容性更好
        .sda_gpio = 17,           // SDA引脚
        .scl_gpio = 18,           // SCL引脚
        .width = 128,             // 屏幕宽度
        .height = 64              // 屏幕高度
    };

    // 初始化SH1106
    sh1106_handle_t display = sh1106_init(&sh1106_config);
    if (!display) {
        printf("SH1106 initialization failed!\n");
        return;
    }
    printf("SH1106 initialized successfully\n");

    // 显示测试内容
    sh1106_show_string(display, 0, 0, "ESP32-S3");
    sh1106_show_string(display, 0, 2, "SH1106 OLED");
    sh1106_show_string(display, 0, 4, "Hello World!");

    // 计数器显示
    int count = 0;
    char count_str[20];
    while (1) {
        sprintf(count_str, "Count: %d", count++);
        sh1106_show_string(display, 0, 6, count_str);
        vTaskDelay(pdMS_TO_TICKS(1000));  // 1秒刷新一次
    }

    // 释放资源 (实际不会执行到这里)
    sh1106_deinit(display);
}
