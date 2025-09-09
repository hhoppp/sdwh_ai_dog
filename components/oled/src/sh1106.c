#include "sh1106.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "font6x8.c"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "sh1106";

// 设备结构体定义
typedef struct {
    uint8_t i2c_port;
    uint8_t i2c_addr;
    uint16_t width;
    uint16_t height;
} sh1106_dev_t;

/**
 * @brief 发送I2C命令
 */
static esp_err_t sh1106_send_command(sh1106_handle_t dev, uint8_t cmd) {
    if (!dev) return ESP_FAIL;
    sh1106_dev_t *device = (sh1106_dev_t *)dev;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (device->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0x00, true);  // 命令模式
    i2c_master_write_byte(cmd_handle, cmd, true);
    i2c_master_stop(cmd_handle);
    
    esp_err_t ret = i2c_master_cmd_begin(device->i2c_port, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
    return ret;
}

/**
 * @brief 发送I2C数据
 */
static esp_err_t sh1106_send_data(sh1106_handle_t dev, uint8_t *data, size_t len) {
    if (!dev || !data || len == 0) return ESP_FAIL;
    sh1106_dev_t *device = (sh1106_dev_t *)dev;

    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (device->i2c_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd_handle, 0x40, true);  // 数据模式
    i2c_master_write(cmd_handle, data, len, true);
    i2c_master_stop(cmd_handle);
    
    esp_err_t ret = i2c_master_cmd_begin(device->i2c_port, cmd_handle, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd_handle);
    return ret;
}

/**
 * @brief 初始化I2C
 */
static esp_err_t i2c_bus_init(sh1106_handle_t dev, const sh1106_config_t *config) {
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->sda_gpio,
        .scl_io_num = config->scl_gpio,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = config->clk_speed,
    };

    esp_err_t ret = i2c_param_config(config->i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(config->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/**
 * @brief 初始化SH1106命令序列
 */
static esp_err_t sh1106_init_sequence(sh1106_handle_t dev) {
    esp_err_t ret;
    
    // 关闭显示
    ret = sh1106_send_command(dev, 0xAE);
    if (ret != ESP_OK) return ret;
    
    // 设置时钟分频和振荡器频率
    ret = sh1106_send_command(dev, 0xD5);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0x80);  // 推荐值
    if (ret != ESP_OK) return ret;
    
    // 设置多路复用比
    ret = sh1106_send_command(dev, 0xA8);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0x3F);  // 64行
    if (ret != ESP_OK) return ret;
    
    // 设置显示偏移
    ret = sh1106_send_command(dev, 0xD3);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0x00);  // 无偏移
    if (ret != ESP_OK) return ret;
    
    // 设置起始行地址
    ret = sh1106_send_command(dev, 0x40);
    if (ret != ESP_OK) return ret;
    
    // 设置段重定向
    ret = sh1106_send_command(dev, 0xA1);
    if (ret != ESP_OK) return ret;
    
    // 设置COM输出扫描方向
    ret = sh1106_send_command(dev, 0xC8);
    if (ret != ESP_OK) return ret;
    
    // 设置COM引脚硬件配置
    ret = sh1106_send_command(dev, 0xDA);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0x12);  // 推荐值
    if (ret != ESP_OK) return ret;
    
    // 设置对比度
    ret = sh1106_send_command(dev, 0x81);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0xCF);  // 高对比度
    if (ret != ESP_OK) return ret;
    
    // 设置预充电周期
    ret = sh1106_send_command(dev, 0xD9);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0xF1);  // 推荐值
    if (ret != ESP_OK) return ret;
    
    // 设置VCOMH取消选择级别
    ret = sh1106_send_command(dev, 0xDB);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0x40);  // 推荐值
    if (ret != ESP_OK) return ret;
    
    // 开启电荷泵
    ret = sh1106_send_command(dev, 0x8D);
    if (ret != ESP_OK) return ret;
    ret = sh1106_send_command(dev, 0x14);  // 使能电荷泵
    if (ret != ESP_OK) return ret;

    // // 设置显示起始列偏移（解决 132 列显存导致的右侧乱码）
    // ret = sh1106_send_command(dev, 0x21);  // 设置列地址范围
    // if (ret != ESP_OK) return ret;
    // ret = sh1106_send_command(dev, 0x02);  // 起始列：2（跳过前 2 列冗余）
    // if (ret != ESP_OK) return ret;
    // ret = sh1106_send_command(dev, 0x7F);  // 结束列：127（共 128 列）
    // if (ret != ESP_OK) return ret;
    
    // 开启显示
    ret = sh1106_send_command(dev, 0xAF);
    if (ret != ESP_OK) return ret;
    
    return ESP_OK;
}

sh1106_handle_t sh1106_init(const sh1106_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter (NULL)");
        return NULL;
    }

    // 分配设备结构体内存
    sh1106_dev_t *dev = (sh1106_dev_t *)malloc(sizeof(sh1106_dev_t));
    if (!dev) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }

    // 初始化设备参数
    dev->i2c_port = config->i2c_port;
    dev->i2c_addr = config->i2c_addr;
    dev->width = config->width;
    dev->height = config->height;

    // 初始化I2C总线
    esp_err_t ret = i2c_bus_init(dev, config);
    if (ret != ESP_OK) {
        free(dev);
        return NULL;
    }

    // 初始化SH1106
    ret = sh1106_init_sequence(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SH1106 initialization failed: %s", esp_err_to_name(ret));
        i2c_driver_delete(dev->i2c_port);
        free(dev);
        return NULL;
    }

    // 清屏
    sh1106_clear(dev);
    return dev;
}

void sh1106_deinit(sh1106_handle_t dev) {
    if (dev) {
        sh1106_dev_t *device = (sh1106_dev_t *)dev;
        i2c_driver_delete(device->i2c_port);
        free(device);
    }
}

esp_err_t sh1106_clear(sh1106_handle_t dev) {
    if (!dev) return ESP_FAIL;
    // sh1106_dev_t *device = (sh1106_dev_t *)dev;

    uint8_t data[128] = {0};
    for (int page = 0; page < 8; page++) {
        // 设置页地址
        esp_err_t ret = sh1106_send_command(dev, 0xB0 + page);
        if (ret != ESP_OK) return ret;
        
        // 设置列地址
        ret = sh1106_send_command(dev, 0x00 + ((2) & 0x0F));  // 低4位
        if (ret != ESP_OK) return ret;
        ret = sh1106_send_command(dev, 0x10 + (((2) >> 4) & 0x0F));  // 高4位
        if (ret != ESP_OK) return ret;
        
        // 填充数据
        ret = sh1106_send_data(dev, data, 128);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t sh1106_show_char(sh1106_handle_t dev, uint8_t x, uint8_t y, char c) {
    if (!dev || x >= 128 || y >= 8) return ESP_FAIL;
    
    // 设置显示位置
    esp_err_t ret = sh1106_send_command(dev, 0xB0 + y);  // 页地址
    if (ret != ESP_OK) return ret;
    
    ret = sh1106_send_command(dev, 0x00 + ((x+2) & 0x0F));  // 列地址低4位
    if (ret != ESP_OK) return ret;
    
    ret = sh1106_send_command(dev, 0x10 + (((x+2) >> 4) & 0x0F));  // 列地址高4位
    if (ret != ESP_OK) return ret;
    
    // 获取字符字模
    uint8_t char_data[6] = {0};
    if (c >= ' ' && c <= '~') {  // 只处理可打印字符
        size_t index = (c - ' ') * 6;
        if (index + 6 <= sizeof(font6x8)) {
            memcpy(char_data, &font6x8[index], 6);
        }
    }
    
    // 发送字符数据
    return sh1106_send_data(dev, char_data, 6);
}

esp_err_t sh1106_show_string(sh1106_handle_t dev, uint8_t x, uint8_t y, const char *str) {
    if (!dev || !str) return ESP_FAIL;
    
    uint8_t current_x = x;
    uint8_t current_y = y;
    
    while (*str) {
        // 检查是否需要换行
        if (current_x + 6 > 128) {
            current_x = 0;
            current_y++;
            if (current_y >= 8) break;  // 超出屏幕范围
        }
        
        // 显示单个字符
        esp_err_t ret = sh1106_show_char(dev, current_x, current_y, *str);
        if (ret != ESP_OK) return ret;
        
        current_x += 6;  // 每个字符宽度为6像素
        str++;
    }
    
    return ESP_OK;
}

esp_err_t sh1106_display_on(sh1106_handle_t dev) {
    return sh1106_send_command(dev, 0xAF);
}

esp_err_t sh1106_display_off(sh1106_handle_t dev) {
    return sh1106_send_command(dev, 0xAE);
}
