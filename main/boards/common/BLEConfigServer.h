#ifndef BLECONFIGSERVER_H
#define BLECONFIGSERVER_H

#pragma once

// struct BLEServerState {
//   bool is_connected = false;
//   bool is_configuring = false;
//   bool is_configured = false;
// };

#define BLE_SERVER_DISCONNECTED  0x00
#define BLE_SERVER_CONFIGURING   0x01
#define BLE_SERVER_CONFIG_FAILED 0x02

class BLEConfigServer {
public:
  BLEConfigServer();
  ~BLEConfigServer();

  static BLEConfigServer &GetInstance();
  void SetPrefix(const char *prefix);
  void Start();

private:
  unsigned char _state = BLE_SERVER_DISCONNECTED;
};

#endif