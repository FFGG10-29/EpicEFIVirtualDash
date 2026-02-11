#include "project_config.h"
#include "ble_manager.h"
#include "can_manager.h"
#include "usb_manager.h"  // 添加USB管理器头文件包含

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("ESP32S3 Car Dashboard Starting...");

  // 初始化BLE管理器
  if (!bleManager.init()) {
    Serial.println("Failed to initialize BLE Manager");
    while (1)
      ;
  }

  // 初始化CAN管理器
  if (!canManager.init()) {
    Serial.println("Failed to initialize CAN Manager");
    while (1)
      ;
  }

  // 初始化USB管理器
  if (!usbManager.begin()) {  // 现在usbManager已正确定义
    Serial.println("Failed to initialize USB Manager");
  }

  // 设置USB到CAN的回调
  usbManager.setCANSendCallback([](uint8_t modifier, uint8_t firstKey, uint8_t secondKey) {
    canManager.sendCommand(modifier, firstKey, secondKey);
  });

  Serial.println("All managers initialized successfully");
}

void loop() {
  // 更新BLE管理器
  bleManager.update();

  // 处理CAN接收
  canManager.processRx();

  // 处理USB任务
  usbManager.task();  // 现在usbManager已正确定义

  delay(10);
}