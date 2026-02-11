#pragma once
#include <Arduino.h>
#include <EspUsbHost.h>

// USB按钮管理器类
class USBBtnManager {
public:
    USBBtnManager();
    bool begin();
    void task();
    
    // 设置CAN发送回调函数
    void setCANSendCallback(void (*callback)(uint8_t, uint8_t, uint8_t)) {
        canSendCallback = callback;
    }

private:
    class MyEspUsbHost : public EspUsbHost {
        USBBtnManager* parent;
    public:
        MyEspUsbHost(USBBtnManager* p) : parent(p) {}
        
        void onGone(const usb_host_client_event_msg_t *eventMsg) {
            if (parent) parent->handleDeviceGone();
        }

        void onReceive(const usb_transfer_t *transfer) {
            if (parent) parent->handleUSBReceive(transfer);
        }
    };

    void handleDeviceGone();
    void handleUSBReceive(const usb_transfer_t *transfer);
    void processHIDData(uint8_t modifier, uint8_t firstKey, uint8_t secondKey);
    
    MyEspUsbHost usbHost;
    void (*canSendCallback)(uint8_t, uint8_t, uint8_t) = nullptr;
    int deviceGoneFlag = 0;
};

// 添加全局实例声明
extern USBBtnManager usbManager;