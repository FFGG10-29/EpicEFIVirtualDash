#include "usb_manager.h"
#include <Arduino.h>

// 添加全局实例定义
USBBtnManager usbManager;

USBBtnManager::USBBtnManager() : usbHost(this) {}

bool USBBtnManager::begin() {
    deviceGoneFlag = 0;
    usbHost.begin();
    usbHost.setHIDLocal(HID_LOCAL_Japan_Katakana);
    usbHost.task();
    Serial.println("USB Manager initialized successfully");
    return true;
}

void USBBtnManager::task() {
    usbHost.task();
}

void USBBtnManager::handleDeviceGone() {
    deviceGoneFlag = 1;
    Serial.println("USB device disconnected");
}

void USBBtnManager::handleUSBReceive(const usb_transfer_t *transfer) {
    if (!transfer->data_buffer) return;

    // 打印接收到的原始数据（调试用）
    Serial.print("USB HID Data: ");
    for (int i = 0; i < transfer->data_buffer_size && i < 50; i++) {
        Serial.printf("%02x ", transfer->data_buffer[i]);
    }
    Serial.println();

    if (transfer->num_bytes > 4 && transfer->data_buffer_size > 4) {
        uint8_t modifier = transfer->data_buffer[0];
        uint8_t firstKey = transfer->data_buffer[2];
        uint8_t secondKey = transfer->data_buffer[3];

        processHIDData(modifier, firstKey, secondKey);
    }
}

void USBBtnManager::processHIDData(uint8_t modifier, uint8_t firstKey, uint8_t secondKey) {
    if (firstKey > 0) firstKey += (modifier * 0xff);
    if (secondKey > 0) secondKey += (modifier * 0xff);

    if (canSendCallback) {
        if (firstKey > 0) {
            canSendCallback(modifier, firstKey & 0xff, (firstKey >> 8) & 0xff);
        } else if (secondKey > 0) {
            canSendCallback(modifier, secondKey & 0xff, (secondKey >> 8) & 0xff);
        }
    }
}