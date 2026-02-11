#include "ble_manager.h"
#include "can_manager.h"

// 全局BLE管理器实例
BleManager bleManager;

// 日志函数定义
void logMessage(const String& message) {
  Serial.println(String(millis()) + "ms: " + message);
}

// ============================================================================
// BLE回调类实现
// ============================================================================

class BleManager::ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    bleManager.handleClientConnected();
  }

  void onDisconnect(BLEServer* pServer) override {
    bleManager.handleClientDisconnected();
  }
};

class BleManager::ButtonCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    bleManager.handleButtonWrite(value);
  }
};

class BleManager::VarRequestCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    bleManager.handleVarRequestWrite(value);
  }
};

class BleManager::VarSetCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    String value = pCharacteristic->getValue();
    bleManager.handleVarSetWrite(value);
  }
};

// ============================================================================
// BleManager 实现
// ============================================================================

BleManager::BleManager() {}

bool BleManager::init() {
  logMessage("BLE Manager: Initializing...");

  BLEDevice::init("ESP32S3 Car Dashboard");
  BLEDevice::setMTU(517);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID), 25);

  // 创建按钮特征
  pButtonChar = pService->createCharacteristic(
    CHAR_BUTTON_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR);
  pButtonChar->setCallbacks(new ButtonCharCallbacks());

  // 创建变量数据特征
  pVarDataChar = pService->createCharacteristic(
    CHAR_VAR_DATA_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);


  // 创建变量请求特征
  pVarRequestChar = pService->createCharacteristic(
    CHAR_VAR_REQUEST_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR);
  pVarRequestChar->setCallbacks(new VarRequestCharCallbacks());

  // 创建GPS数据特征
  pGpsDataChar = pService->createCharacteristic(
    CHAR_GPS_DATA_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR);
  pGpsDataChar->setCallbacks(new VarSetCharCallbacks());

  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);

  BLEDevice::startAdvertising();
  logMessage("BLE Manager: Server started and advertising");

  return true;
}

void BleManager::update() {
  checkRequestTimeout();

  // 处理连接状态变化
  if (!deviceConnected && oldDeviceConnected) {
    static uint32_t disconnectTime = 0;

    if (disconnectTime == 0) {
      disconnectTime = millis();
    } else if (millis() - disconnectTime >= RECONNECT_DELAY_MS) {
      pServer->startAdvertising();
      logMessage("BLE Manager: Restarted advertising");
      oldDeviceConnected = deviceConnected;
      disconnectTime = 0;
    }
  } else {
    if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;

      // 重置批量请求状态
      pendingVarCount = 0;
      pendingVarIndex = 0;
      batchResponseCount = 0;
    }
  }
}

void BleManager::handleClientConnected() {
  deviceConnected = true;
  logMessage("BLE Manager: Client connected");
}

void BleManager::handleClientDisconnected() {
  deviceConnected = false;
  logMessage("BLE Manager: Client disconnected");
}

void BleManager::handleButtonWrite(const String& value) {
  if (value.length() >= 2) {
    uint16_t buttonMask = (uint8_t)value[0] | ((uint8_t)value[1] << 8);
    if (buttonMask != lastButtonMask) {
      lastButtonMask = buttonMask;
      canManager.sendButtonFrame(buttonMask);
    }
  } else if (value.length() == 1) {
    uint8_t buttonId = (uint8_t)value[0];
    uint16_t buttonMask = (1 << buttonId);
    if (buttonMask != lastButtonMask) {
      lastButtonMask = buttonMask;
      canManager.sendButtonFrame(buttonMask);
    }
  }
}

void BleManager::handleVarRequestWrite(const String& value) {
  size_t len = value.length();

  if (len < 4) {
    logMessage("BLE Manager: Variable request too short");
    return;
  }

  // 如果当前有未完成的请求，忽略新请求
  if (pendingVarCount > 0 && pendingVarIndex < pendingVarCount) {
    logMessage("BLE Manager: Previous batch still in progress");
    return;
  }

  // 开始新的批量请求
  startBatchRequest();

  // 解析变量哈希值
  for (size_t i = 0; i + 4 <= len && pendingVarCount < MAX_BATCH_VARS; i += 4) {
    int32_t varHash = readInt32BigEndian((const uint8_t*)(value.c_str() + i));
    addVariableToBatch(varHash);
  }

  if (pendingVarCount > 0) {
    lastVarRequestTime = millis();
    canManager.requestVariable(pendingVarHashes[0]);
    logMessage("BLE Manager: Started batch request with " + String(pendingVarCount) + " variables");
  }
}

void BleManager::handleVarSetWrite(const String& value) {
  size_t len = value.length();

  if (len < 8) {
    logMessage("BLE Manager: Variable set data too short");
    return;
  }

  for (size_t i = 0; i + 8 <= len; i += 8) {
    const uint8_t* data = (const uint8_t*)value.c_str() + i;
    int32_t varHash = readInt32BigEndian(data);
    float varValue = readFloat32BigEndian(data + 4);

    canManager.sendVariableToEcu(varHash, varValue);
  }
}

void BleManager::startBatchRequest() {
  pendingVarCount = 0;
  pendingVarIndex = 0;
  batchResponseCount = 0;
}

void BleManager::addVariableToBatch(int32_t varHash) {
  if (pendingVarCount < MAX_BATCH_VARS) {
    pendingVarHashes[pendingVarCount++] = varHash;
  }
}

void BleManager::sendBatchResponse() {
  if (batchResponseCount == 0) return;
  if (!deviceConnected || pVarDataChar == nullptr) return;

  uint32_t now = millis();
  if (now - lastBleNotifyTime < BLE_NOTIFY_MIN_INTERVAL_MS) return;

  pVarDataChar->setValue(batchResponseBuffer, batchResponseCount * VAR_RESPONSE_SIZE);
  pVarDataChar->notify();

  lastBleNotifyTime = now;
  bleNotifyCount++;

  logMessage("BLE Manager: Sent batch response with " + String(batchResponseCount) + " variables");

  // 重置状态
  startBatchRequest();
}

void BleManager::checkRequestTimeout() {
  if (pendingVarCount == 0 || pendingVarIndex >= pendingVarCount) return;

  uint32_t now = millis();
  if (now - lastVarRequestTime >= VAR_REQUEST_TIMEOUT_MS) {
    timeoutCount++;
    logMessage("BLE Manager: Variable request timeout for index " + String(pendingVarIndex));

    pendingVarIndex++;

    if (pendingVarIndex < pendingVarCount) {
      lastVarRequestTime = now;
      canManager.requestVariable(pendingVarHashes[pendingVarIndex]);
    } else {
      sendBatchResponse();
    }
  }
}