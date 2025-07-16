#include "wifi_board.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_http.h>
#include <esp_mqtt.h>
#include <esp_udp.h>
#include <tcp_transport.h>
#include <tls_transport.h>
#include <web_socket.h>
#include <esp_log.h>
#include <cJSON.h>
#include <wifi_station.h>
#include <wifi_configuration_ap.h>
#include <ssid_manager.h>
#include "settings.h"
#include "system_info.h"
#include "application.h"

static const char *TAG = "WifiBoard";

WifiBoard::WifiBoard() {
    Settings settings("wifi", true);
    wifi_config_mode_ = settings.GetInt("force_ap") == 1;
    ESP_LOGI(TAG, "WifiBoard 初始化，force_ap = %d", wifi_config_mode_);
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "force_ap 为 1，重置为 0");
        settings.SetInt("force_ap", 0);
    }
}

std::string WifiBoard::GetBoardType() {
    ESP_LOGI(TAG, "获取板子类型：wifi");
    return "wifi";
}

void WifiBoard::EnterWifiConfigMode() {
    ESP_LOGI(TAG, "进入 WiFi 配置模式");
    auto& application = Application::GetInstance();
    application.SetDeviceState(kDeviceStateWifiConfiguring);

    auto& wifi_ap = WifiConfigurationAp::GetInstance();
    wifi_ap.SetSsidPrefix("Xiaozhi");
    ESP_LOGI(TAG, "启动 WiFi 配置 AP，SSID 前缀：Xiaozhi");
    wifi_ap.Start();

    // Wait forever until reset after configuration
    while (true) {
        int free_sram = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        int min_free_sram = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "内存状态 - 可用内部 SRAM: %u, 最小可用 SRAM: %u", free_sram, min_free_sram);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void WifiBoard::StartNetwork() {
    ESP_LOGI(TAG, "开始启动网络");
    // User can press BOOT button while starting to enter WiFi configuration mode
    if (wifi_config_mode_) {
        ESP_LOGI(TAG, "检测到 wifi_config_mode_ 为 true，进入配置模式");
        EnterWifiConfigMode();
        return;
    }

    // If no WiFi SSID is configured, enter WiFi configuration mode
    auto& ssid_manager = SsidManager::GetInstance();
    auto ssid_list = ssid_manager.GetSsidList();
    if (ssid_list.empty()) {
        ESP_LOGI(TAG, "未找到配置的 WiFi SSID，进入配置模式");
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }
    ESP_LOGI(TAG, "找到 %u 个配置的 SSID，尝试连接 WiFi", ssid_list.size());

    auto& wifi_station = WifiStation::GetInstance();
    ESP_LOGI(TAG, "启动 WiFi 站点模式");
    wifi_station.Start();

    // Try to connect to WiFi, if failed, launch the WiFi configuration AP
    ESP_LOGI(TAG, "等待 WiFi 连接，最长等待 60 秒");
    if (!wifi_station.WaitForConnected(60 * 1000)) {
        ESP_LOGI(TAG, "WiFi 连接失败，停止站点模式并进入配置模式");
        wifi_station.Stop();
        wifi_config_mode_ = true;
        EnterWifiConfigMode();
        return;
    }
    ESP_LOGI(TAG, "WiFi 连接成功，SSID: %s", wifi_station.GetSsid().c_str());
}

Http* WifiBoard::CreateHttp() {
    ESP_LOGI(TAG, "创建 HTTP 实例");
    return new EspHttp();
}

WebSocket* WifiBoard::CreateWebSocket() {
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    ESP_LOGI(TAG, "创建 WebSocket 实例，URL: %s", url.c_str());
    if (url.find("wss://") == 0) {
        ESP_LOGI(TAG, "检测到 wss:// 协议，使用 TLS 传输");
        return new WebSocket(new TlsTransport());
    } else {
        ESP_LOGI(TAG, "使用 TCP 传输");
        return new WebSocket(new TcpTransport());
    }
    return nullptr;
}

Mqtt* WifiBoard::CreateMqtt() {
    ESP_LOGI(TAG, "创建 MQTT 实例");
    return new EspMqtt();
}

Udp* WifiBoard::CreateUdp() {
    ESP_LOGI(TAG, "创建 UDP 实例");
    return new EspUdp();
}

std::string WifiBoard::GetBoardJson() {
    ESP_LOGI(TAG, "生成板子 JSON 数据");
    auto& wifi_station = WifiStation::GetInstance();
    std::string board_json = R"({)";
    board_json += R"("type":")" + std::string("esp-box") + R"(",)";
    board_json += R"("name":")" + std::string("esp-box") + R"(",)";
    if (!wifi_config_mode_) {
        board_json += R"("ssid":")" + wifi_station.GetSsid() + R"(",)";
        board_json += R"("rssi":)" + std::to_string(wifi_station.GetRssi()) + R"(,)";
        board_json += R"("channel":)" + std::to_string(wifi_station.GetChannel()) + R"(,)";
        board_json += R"("ip":")" + wifi_station.GetIpAddress() + R"(",)";
    }
    board_json += R"("mac":")" + SystemInfo::GetMacAddress() + R"(")";
    board_json += R"(})";
    ESP_LOGI(TAG, "板子 JSON 数据生成完成: %s", board_json.c_str());
    return board_json;
}

void WifiBoard::SetPowerSaveMode(bool enabled) {
    ESP_LOGI(TAG, "设置 WiFi 省电模式: %s", enabled ? "开启" : "关闭");
    auto& wifi_station = WifiStation::GetInstance();
    wifi_station.SetPowerSaveMode(enabled);
}

void WifiBoard::ResetWifiConfiguration() {
    ESP_LOGI(TAG, "重置 WiFi 配置");
    {
        Settings settings("wifi", true);
        settings.SetInt("force_ap", 1);
        ESP_LOGI(TAG, "设置 force_ap = 1，将重启进入配置模式");
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "准备重启设备");
    esp_restart();
}

std::string WifiBoard::GetDeviceStatusJson() {
    ESP_LOGI(TAG, "生成设备状态 JSON 数据");
    auto root = cJSON_CreateObject();

    // Network
    auto network = cJSON_CreateObject();
    auto& wifi_station = WifiStation::GetInstance();
    cJSON_AddStringToObject(network, "type", "wifi");
    cJSON_AddStringToObject(network, "ssid", wifi_station.GetSsid().c_str());
    int rssi = wifi_station.GetRssi();
    if (rssi >= -60) {
        cJSON_AddStringToObject(network, "signal", "strong");
    } else if (rssi >= -70) {
        cJSON_AddStringToObject(network, "signal", "medium");
    } else {
        cJSON_AddStringToObject(network, "signal", "weak");
    }
    cJSON_AddItemToObject(root, "network", network);

    auto json_str = cJSON_PrintUnformatted(root);
    std::string json(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    ESP_LOGI(TAG, "设备状态 JSON 数据生成完成: %s", json.c_str());
    return json;
}