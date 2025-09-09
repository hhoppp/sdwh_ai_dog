#ifndef __SH1106_H__
#define __SH1106_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// SH1106配置结构体
typedef struct {
    uint8_t i2c_port;       // I2C端口号 (0或1)
    uint8_t i2c_addr;       // I2C设备地址 (0x3C或0x3D)
    uint32_t clk_speed;     // I2C时钟频率 (通常100000或400000)
    int sda_gpio;           // SDA引脚号
    int scl_gpio;           // SCL引脚号
    uint16_t width;         // 屏幕宽度 (通常128)
    uint16_t height;        // 屏幕高度 (通常64)
} sh1106_config_t;

// SH1106设备句柄
typedef void* sh1106_handle_t;

/**
 * @brief 初始化SH1106显示屏
 * @param config 配置参数
 * @return 成功返回设备句柄，失败返回NULL
 */
sh1106_handle_t sh1106_init(const sh1106_config_t *config);

/**
 * @brief 释放SH1106资源
 * @param dev 设备句柄
 */
void sh1106_deinit(sh1106_handle_t dev);

/**
 * @brief 清空屏幕
 * @param dev 设备句柄
 * @return 成功返回ESP_OK
 */
esp_err_t sh1106_clear(sh1106_handle_t dev);

/**
 * @brief 显示单个字符
 * @param dev 设备句柄
 * @param x 起始X坐标 (0-127)
 * @param y 起始Y坐标 (0-7，按页计算)
 * @param c 要显示的字符
 * @return 成功返回ESP_OK
 */
esp_err_t sh1106_show_char(sh1106_handle_t dev, uint8_t x, uint8_t y, char c);

/**
 * @brief 显示字符串
 * @param dev 设备句柄
 * @param x 起始X坐标
 * @param y 起始Y坐标
 * @param str 要显示的字符串
 * @return 成功返回ESP_OK
 */
esp_err_t sh1106_show_string(sh1106_handle_t dev, uint8_t x, uint8_t y, const char *str);

/**
 * @brief 开启显示
 * @param dev 设备句柄
 * @return 成功返回ESP_OK
 */
esp_err_t sh1106_display_on(sh1106_handle_t dev);

/**
 * @brief 关闭显示
 * @param dev 设备句柄
 * @return 成功返回ESP_OK
 */
esp_err_t sh1106_display_off(sh1106_handle_t dev);

#endif  // __SH1106_H__
