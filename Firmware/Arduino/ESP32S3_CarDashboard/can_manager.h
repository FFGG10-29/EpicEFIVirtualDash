#ifndef CAN_MANAGER_H
#define CAN_MANAGER_H

#include "project_config.h"
#include <ACAN2515.h>
#include <SPI.h>

class CanManager {
public:
    // 构造函数和初始化
    CanManager();
    bool init();
    
    // CAN发送功能
    bool sendButtonFrame(uint16_t buttonMask);
    bool requestVariable(int32_t varHash);
    bool sendVariableToEcu(int32_t varHash, float value);
    
    // CAN接收处理
    void processRx();
    
    // 统计信息
    uint32_t getTxCount() const { return canTxCount; }
    uint32_t getRxCount() const { return canRxCount; }
    
    // 设置数据回调函数
    void setRxCallback(void (*callback)(const uint8_t* data, uint8_t len));

private:
    ACAN2515 can;
    uint32_t canTxCount = 0;
    uint32_t canRxCount = 0;
    void (*rxCallback)(const uint8_t* data, uint8_t len) = nullptr;
    
    void handleReceivedFrame(const CANMessage& frame);
};

extern CanManager canManager;

#endif // CAN_MANAGER_H