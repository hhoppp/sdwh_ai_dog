#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_module.h"

#define TAG "WIFI_MODULE"
#define WIFI_SCAN_MAX_AP 30    // 最大扫描AP数量
#define WIFI_CONNECT_TIMEOUT 30 // 连接超时时间（秒）

// 全局变量：网络接口、连接状态、扫描信号量（解决中断冲突）
static esp_netif_t *s_netif = NULL;
static bool s_is_connected = false;
static SemaphoreHandle_t s_scan_sem = NULL; // 扫描完成信号量


/**
 * @brief WiFi 事件处理回调（统一处理连接、断开、IP获取事件）
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA mode started, trying to connect...");
                esp_wifi_connect(); // 启动后自动连接
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                s_is_connected = false;
                ESP_LOGW(TAG, "WiFi disconnected, reconnect after 3s...");
                vTaskDelay(pdMS_TO_TICKS(3000)); // 延迟重连，避免频繁重试
                esp_wifi_connect();
                break;
            case WIFI_EVENT_SCAN_DONE:
                // 扫描完成，释放信号量（中断上下文安全）
                if (s_scan_sem != NULL) {
                    xSemaphoreGiveFromISR(s_scan_sem, NULL);
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // 获取IP地址成功
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_is_connected = true;
    }
}


/**
 * @brief WiFi 模块初始化（修复配置警告、创建扫描信号量）
 */
esp_err_t wifi_module_init(void) {
    // 1. 初始化NVS（存储WiFi配置）
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t nvs_handle;
    if (nvs_open("wifi_config", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_erase_all(nvs_handle); // 清除 WiFi 相关配置
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    // 2. 初始化网络接口（默认STA模式）
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif = esp_netif_create_default_wifi_sta(); // 创建STA网络接口

    // 3. 初始化WiFi驱动（修复配置警告：允许开放网络连接）
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. 注册事件处理（包括扫描完成事件）
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // 5. 配置WiFi STA参数（关键：允许开放网络，避免密码为空警告）
    wifi_config_t wifi_config = {
        .sta = {
            // 认证模式阈值：支持开放网络（0）到WPA2（3）的所有模式
            .threshold.authmode = WIFI_AUTH_OPEN, 
            .pmf_cfg = { // 启用PMF（保护管理帧），增强兼容性
                .capable = true,
                .required = false,
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 6. 创建扫描信号量（中断安全，解决扫描回调冲突）
    s_scan_sem = xSemaphoreCreateBinary();
    if (s_scan_sem == NULL) {
        ESP_LOGE(TAG, "Create scan semaphore failed");
        return ESP_ERR_NO_MEM;
    }

    // 7. 启动WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi module initialized (STA mode)");
    return ESP_OK;
}


/**
 * @brief WiFi 扫描AP（修复中断冲突，支持稳定扫描）
 */
esp_err_t wifi_module_scan_ap(void) {

    // 等待 WiFi 启动完成（最多等待 3 秒）
    int retry = 0;
    while (retry < 30) {
        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);
        if (mode == WIFI_MODE_STA) {
            break; // WiFi 已就绪
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }
    if (retry >= 30) {
        ESP_LOGE(TAG, "WiFi STA mode not ready");
        return ESP_ERR_TIMEOUT;
    }

    if (s_scan_sem == NULL) {
        ESP_LOGE(TAG, "Scan semaphore not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 1. 清除之前的扫描结果
    ESP_ERROR_CHECK(esp_wifi_scan_stop());

    // 2. 设置国家
    //在所有信道中扫描全部 AP（前端）
    wifi_country_t country_config = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
    };
    esp_wifi_set_country(&country_config); // 4.1 扫描配置国家代码

    // 3. 配置扫描参数（非阻塞模式，扫描完成后触发WIFI_EVENT_SCAN_DONE）

    wifi_active_scan_time_t active={
            .min = 200,
            .max = 400
        }; // 每个信道主动扫描时间（ms）

    wifi_scan_time_t scan_time= {
        .active = active,
        .passive = 100 // 每个信道被动扫描时间（ms）
    };

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,        // 不指定SSID，扫描所有AP
        .bssid = NULL,       // 不指定BSSID
        .channel = 0,        // 扫描所有信道（0-13）
        .show_hidden = true, // 显示隐藏AP
        .scan_type = WIFI_SCAN_TYPE_ACTIVE, // 主动扫描（更准确）
        .scan_time = scan_time 
    };

    // 4. 启动扫描（非阻塞模式，避免阻塞任务）
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Start scan failed: %s", esp_err_to_name(err));
        return err;
    }

    // 5. 等待扫描完成（超时5秒，避免无限阻塞）
    if (xSemaphoreTake(s_scan_sem, pdMS_TO_TICKS(10000)) == pdFALSE) {
        ESP_LOGE(TAG, "Scan timeout (5s)");
        return ESP_ERR_TIMEOUT;
    }

    // 6. 获取扫描结果（动态内存分配：避免栈上大数组占用过多栈空间）
    uint16_t ap_count = 0;
    // 第一步：获取AP数量
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (ap_count == 0) {
        ESP_LOGW(TAG, "Found 0 access points");
        return ESP_OK;
    }

    // 第二步：动态分配内存存储AP信息（ap_count 最大30，内存可控）
    wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(ap_count * sizeof(wifi_ap_record_t));
    if (ap_list == NULL) {
        ESP_LOGE(TAG, "Malloc AP list failed");
        return ESP_ERR_NO_MEM;
    }

    // 第三步：读取AP信息并打印
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "Found %d access points:", ap_count);
    ESP_LOGI(TAG, "---------------------------------");
    ESP_LOGI(TAG, "No. | SSID                  | RSSI (dBm) | Auth Mode");
    ESP_LOGI(TAG, "---------------------------------");
    for (int i = 0; i < ap_count; i++) {
        const char *rssi_level = (ap_list[i].rssi > -50) ? "Excellent" :
                                 (ap_list[i].rssi > -70) ? "Good" :
                                 (ap_list[i].rssi > -90) ? "Fair" : "Weak";
        
        const char *auth_mode_str = NULL;
        switch (ap_list[i].authmode) {
            case WIFI_AUTH_OPEN: auth_mode_str = "Open"; break;
            case WIFI_AUTH_WEP: auth_mode_str = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: auth_mode_str = "WPA-PSK"; break;
            case WIFI_AUTH_WPA2_PSK: auth_mode_str = "WPA2-PSK"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: auth_mode_str = "WPA/WPA2-PSK"; break;
            default: auth_mode_str = "Unknown"; break;
        }

        ESP_LOGI(TAG, "%3d | %-24s | %-10d | %s",
                 i + 1, ap_list[i].ssid, ap_list[i].rssi, auth_mode_str);
    }
    ESP_LOGI(TAG, "=================================");

    // 7. 释放动态内存（避免内存泄漏）
    free(ap_list);
    return ESP_OK;
}


/**
 * @brief WiFi 连接指定AP（支持超时检测）
 */
esp_err_t wifi_module_connect(const char *ssid, const char *password) {
    if (ssid == NULL || strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID is empty");
        return ESP_ERR_INVALID_ARG;
    }

    // 1. 配置目标AP参数
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_OPEN, // 兼容所有认证模式
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };
    // 复制SSID和密码（避免缓冲区溢出）
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password != NULL) {
        strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }

    // 2. 设置AP配置并连接
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_LOGI(TAG, "Connecting to SSID: %s (password: %s)", ssid, (password ? "***" : "None"));
    ESP_ERROR_CHECK(esp_wifi_connect());

    // 3. 等待连接成功（超时检测）
    int timeout_cnt = 0;
    while (!s_is_connected && timeout_cnt < WIFI_CONNECT_TIMEOUT) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        timeout_cnt++;
    }

    if (s_is_connected) {
        ESP_LOGI(TAG, "Connect to %s success", ssid);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Connect timeout (%d s)", WIFI_CONNECT_TIMEOUT);
        return ESP_ERR_TIMEOUT;
    }
}


/**
 * @brief 获取设备IP地址（兼容最新esp_netif接口）
 */
esp_err_t wifi_module_get_ip(char *ip_str, size_t len) {
    if (ip_str == NULL || len < 16) { // IPv4地址最长15字符（如255.255.255.255）
        ESP_LOGE(TAG, "Invalid IP buffer (len >=16 required)");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_is_connected) {
        strlcpy(ip_str, "Not connected", len);
        return ESP_ERR_WIFI_NOT_CONNECT;
    }

    // 使用esp_netif获取IP（替代已弃用的tcpip_adapter）
    esp_netif_ip_info_t ip_info;
    esp_err_t err = esp_netif_get_ip_info(s_netif, &ip_info);
    if (err != ESP_OK) {
        strlcpy(ip_str, "Get IP failed", len);
        ESP_LOGE(TAG, "Get IP info failed: %s", esp_err_to_name(err));
        return err;
    }

    // 格式化IP地址为字符串
    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}


/**
 * @brief 获取当前连接的WiFi信号强度（RSSI）
 */
int8_t wifi_module_get_rssi(void) {
    if (!s_is_connected) {
        ESP_LOGW(TAG, "WiFi not connected, RSSI invalid");
        return -127; // 无效RSSI值（-127表示未连接）
    }

    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Get AP info failed: %s", esp_err_to_name(err));
        return -127;
    }

    // 打印信号强度等级（辅助调试）
    const char *rssi_level = (ap_info.rssi > -50) ? "Excellent" :
                             (ap_info.rssi > -70) ? "Good" :
                             (ap_info.rssi > -90) ? "Fair" : "Weak";
    ESP_LOGI(TAG, "Current RSSI: %d dBm (%s)", ap_info.rssi, rssi_level);
    return ap_info.rssi;
}


/**
 * @brief WiFi 断开连接
 */
esp_err_t wifi_module_disconnect(void) {
    esp_err_t err = esp_wifi_disconnect();
    if (err == ESP_OK) {
        s_is_connected = false;
        ESP_LOGI(TAG, "WiFi disconnected");
    } else {
        ESP_LOGE(TAG, "Disconnect failed: %s", esp_err_to_name(err));
    }
    return err;
}


/**
 * @brief WiFi 模块反初始化（释放资源，适用于程序退出）
 */
esp_err_t wifi_module_deinit(void) {
    // 1. 断开连接并停止WiFi
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());

    // 2. 注销事件处理
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));

    // 3. 释放网络接口和信号量
    if (s_netif != NULL) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    if (s_scan_sem != NULL) {
        vSemaphoreDelete(s_scan_sem);
        s_scan_sem = NULL;
    }

    // 4. 反初始化WiFi驱动和NVS
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(nvs_flash_deinit());
    ESP_LOGI(TAG, "WiFi module deinitialized");
    return ESP_OK;
}