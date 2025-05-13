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

static uint8_t prop_buf[256] = {0};
static ble_server_config ble_config{};

void BLEConfigServer::Start() {
  ble_config.prop_buf = prop_buf;
  ble_config.prop_len = sizeof(prop_buf);
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
      ESP_LOGI(TAG, "Save SSID %s %s", ssid, password);
      SsidManager::GetInstance().AddSsid((const char *)ssid,
                                         (const char *)password);
    }

    if (data[3] == 0x01) {
      int *idx = (int *)data + 1;
      ESP_LOGI(TAG, "Set default SSID %d", *idx);
      SsidManager::GetInstance().SetDefaultSsid(*idx);
    }

    if (data[3] == 0x02) {
      int *idx = (int *)data + 1;
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
    auto &ssid_manager = SsidManager::GetInstance();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "state", cJSON_CreateNumber(inst._state));
    cJSON *array = cJSON_CreateArray();
    for (auto &item : ssid_manager.GetSsidList()) {
      cJSON_AddItemToArray(array, cJSON_CreateString(item.ssid.c_str()));
    }
    cJSON_AddItemToObject(root, "list", array);
    auto *output = cJSON_Print(root);
    memset(prop_buf, 0, sizeof(prop_buf));
    memcpy(prop_buf, output, strlen(output));
    cJSON_Delete(root);
    free(output);
  };
  ble_server_init(&ble_config);
}