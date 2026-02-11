#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include "project_config.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>


class BleManager {
public:
    // 构造函数和初始化
    BleManager();
    bool init();
    void update();
    
    // 连接状态
    bool isConnected() const { return deviceConnected; }
    
    // 批量请求管理
    void startBatchRequest();
    void addVariableToBatch(int32_t varHash);
    void sendBatchResponse();
    
    // 统计信息
    uint32_t getNotifyCount() const { return bleNotifyCount; }
    uint32_t getTimeoutCount() const { return timeoutCount; }
    
    // 批量请求数据（供外部访问）
    int32_t pendingVarHashes[MAX_BATCH_VARS];
    uint8_t pendingVarCount = 0;
    uint8_t pendingVarIndex = 0;
    uint32_t lastVarRequestTime = 0;
    
    uint8_t batchResponseBuffer[MAX_BATCH_VARS * VAR_RESPONSE_SIZE];
    uint8_t batchResponseCount = 0;
    
private:
    BLEServer* pServer = nullptr;
    BLECharacteristic* pButtonChar = nullptr;
    BLECharacteristic* pVarDataChar = nullptr;
    BLECharacteristic* pVarRequestChar = nullptr;
    BLECharacteristic* pGpsDataChar = nullptr;
    
    bool deviceConnected = false;
    bool oldDeviceConnected = false;
    uint16_t lastButtonMask = 0;
    uint32_t lastBleNotifyTime = 0;
    
    uint32_t bleNotifyCount = 0;
    uint32_t timeoutCount = 0;
    
    // BLE回调类
    class ServerCallbacks;
    class ButtonCharCallbacks;
    class VarRequestCharCallbacks;
    class VarSetCharCallbacks;
    
    // 事件处理函数
    void handleButtonWrite(const String& value);
    void handleVarRequestWrite(const String& value);
    void handleVarSetWrite(const String& value);
    void handleClientConnected();
    void handleClientDisconnected();
    
    // 超时检查
    void checkRequestTimeout();
};

extern BleManager bleManager;

#endif // BLE_MANAGER_H