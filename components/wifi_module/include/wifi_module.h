#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include "esp_err.h"
#include "esp_wifi_types.h"

// WiFi 初始化
esp_err_t wifi_module_init(void);
// 扫描并显示 WiFi 列表
esp_err_t wifi_module_scan_ap(void);
// WiFi 连接指定网络
esp_err_t wifi_module_connect(const char *ssid, const char *password);
// 查询设备 IP 地址
esp_err_t wifi_module_get_ip(char *ip_str, size_t len);
// 获取当前连接的 WiFi 信号强度
int8_t wifi_module_get_rssi(void);
// WiFi 断开连接
esp_err_t wifi_module_disconnect(void);

#endif /* WIFI_MODULE_H */