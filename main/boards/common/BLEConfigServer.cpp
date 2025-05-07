#include "BLEConfigServer.h"
#include "ble_server.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_system.h"
#include "ssid_manager.h"
#include <cstring>

static const char *TAG = "BLEConfigServer";

BLEConfigServer::BLEConfigServer() {}

BLEConfigServer::~BLEConfigServer() {}

BLEConfigServer &BLEConfigServer::GetInstance() {
  static BLEConfigServer instance;
  return instance;
}

void BLEConfigServer::SetPrefix(const char *prefix) {}

static ble_server_config ble_config{};

void BLEConfigServer::Start() {
  ble_config.prop_buf = &_state;
  ble_config.prop_len = 1;
  ble_config.write_cb = [](const uint8_t *data, uint16_t len) {
    if (len != 36) {
      return;
    }

    // 命令字： 0 设置ssid，1 设置默认ssid，2 删除ssid, 3 重启

    if (data[0] != 0x55 || data[1] != 0xAA || data[2] != 0x55) {
      return;
    }

    if (data[3] == 0x00) {
      unsigned char *ssid = (unsigned char *)data + 4;
      unsigned char *password = (unsigned char *)data + 20;
      ESP_LOGI(TAG, "Save SSID %s %d", ssid, strlen((const char *)ssid));
      SsidManager::GetInstance().AddSsid((const char *)ssid,
                                         (const char *)password);
    }

    if (data[3] == 0x01) {
      int *idx = (int *)data + 4;
      ESP_LOGI(TAG, "Set default SSID %d", *idx);
      SsidManager::GetInstance().SetDefaultSsid(*idx);
    }

    if (data[3] == 0x02) {
      int *idx = (int *)data + 4;
      ESP_LOGI(TAG, "Remove SSID %d", *idx);
      SsidManager::GetInstance().RemoveSsid(*idx);
    }

    if (data[3] == 0x03) {
      ESP_LOGI(TAG, "Reboot");
      esp_restart();
    }
  };
  ble_config.read_cb = []() {
    auto &inst = BLEConfigServer::GetInstance();
    ESP_LOGI(TAG, "read_cb: %d", inst._state);
  };
  ble_server_init(&ble_config);
}