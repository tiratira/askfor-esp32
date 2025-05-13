#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 蓝牙服务写回调函数
 *
 * @param data 数据指针
 * @param len 数据长度
 */
typedef void (*ble_server_write_cb_t)(const uint8_t *data, uint16_t len);

/**
 * @brief 蓝牙服务读回调函数
 *
 * @param out_buf 输出缓冲区指针
 * @param len 输出缓冲区长度
 */
typedef void (*ble_server_read_cb_t)();

/**
 * @brief 蓝牙服务配置结构体
 *
 */
typedef struct ble_server_config_t {
  uint8_t *prop_buf;              // 属性表指针
  uint16_t prop_len;              // 属性表长度
  ble_server_write_cb_t write_cb; // 写回调函数指针
  ble_server_read_cb_t read_cb;   // 读回调函数指针
} ble_server_config;

/**
 * @brief 初始化蓝牙服务，传入属性表和属性表长度
 *
 * @param ble_config 蓝牙服务配置结构体
 */
void ble_server_init(const ble_server_config *ble_config);

#ifdef __cplusplus
}
#endif