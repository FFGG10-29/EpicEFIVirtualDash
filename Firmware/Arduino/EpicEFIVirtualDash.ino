#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ACAN2515.h>  // 使用ACAN2515替代CANfetti
#include <SPI.h>       // 添加SPI支持
#include <esp_task_wdt.h>

// 调试配置
#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif

// 硬件引脚定义
#define RGB_PIN 38
#define ADC1_PIN 5
#define DIGITAL_INPUT_PIN 1

// MCP2515引脚配置 [1](@ref)
static const byte MCP2515_CS = 10;                               // CS引脚
static const byte MCP2515_INT = 3;                               // 中断引脚
static const uint32_t QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL;  // 8MHz晶振，根据实际模块调整 [3](@ref)

// ACAN2515实例 [1](@ref)
ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);

// 硬件ADC配置
#define ADC1_SAMPLE_INTERVAL_MS 10
#define ADC1_FILTER_SAMPLES 1
#define ADC1_CHANGE_THRESHOLD 5

// 超时和看门狗配置
#define VAR_REQUEST_TIMEOUT_MS 100
#define BLE_NOTIFY_MIN_INTERVAL_MS 10
#define RECONNECT_DELAY_MS 100

#define TS_HW_BUTTONBOX1_CATEGORY 27
#define CANBUS_BUTTONBOX_ADDRESS 0x711

// CAN协议定义
#define ECU_ID 1
#define CAN_VAR_REQUEST_BASE 0x700
#define CAN_VAR_RESPONSE_BASE 0x720
#define CAN_GPS_DATA_BASE 0x780

// GPS变量哈希定义
const int32_t VAR_HASH_GPS_HMSD_PACKED = 703958849;
const int32_t VAR_HASH_GPS_MYQSAT_PACKED = -1519914092;
const int32_t VAR_HASH_GPS_ACCURACY = -1489698215;
const int32_t VAR_HASH_GPS_ALTITUDE = -2100224086;
const int32_t VAR_HASH_GPS_COURSE = 1842893663;
const int32_t VAR_HASH_GPS_LATITUDE = 1524934922;
const int32_t VAR_HASH_GPS_LONGITUDE = -809214087;
const int32_t VAR_HASH_GPS_SPEED = -1486968225;

// 虚拟ADC变量哈希
const int32_t VAR_HASH_ADC[16] = {
  595545759,    // A0
  595545760,    // A1
  595545761,    // A2
  595545762,    // A3
  595545763,    // A4
  595545764,    // A5
  595545765,    // A6
  595545766,    // A7
  595545767,    // A8
  595545768,    // A9
  -1821826352,  // A10
  -1821826351,  // A11
  -1821826350,  // A12
  -1821826349,  // A13
  -1821826348,  // A14
  -1821826347   // A15
};

// Digital input variable hash (D22-D37 packed as 16-bit bitmask)
// Currently only reading IO1 (touch sensor)
const int32_t VAR_HASH_D22_D37 = 2138825443;  // TODO: Replace with actual hash from ECU


// 数字输入配置
#define DIGITAL_SAMPLE_INTERVAL_MS 20
#define DIGITAL_DEBOUNCE_MS 50

// BLE UUID定义 [7,8](@ref)
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_BUTTON_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_VAR_DATA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_VAR_REQUEST_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_GPS_DATA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"

// 函数声明
void setupCan();
void setupBLE();
void sendButtonCanFrame(uint16_t buttonMask);
void requestCanVariable(int32_t varHash);
void sendVariableToEcu(int32_t varHash, float value);
void processCanRx();
void logMessage(const String& message);
void sampleHardwareAdc1();
void sampleDigitalInputs();

// 字节序转换辅助函数（保持不变）
static inline void writeInt32BigEndian(int32_t value, uint8_t* out) {
  out[0] = (uint8_t)((value >> 24) & 0xFF);
  out[1] = (uint8_t)((value >> 16) & 0xFF);
  out[2] = (uint8_t)((value >> 8) & 0xFF);
  out[3] = (uint8_t)(value & 0xFF);
}

static inline int32_t readInt32BigEndian(const uint8_t* in) {
  return ((int32_t)in[0] << 24) | ((int32_t)in[1] << 16) | ((int32_t)in[2] << 8) | (int32_t)in[3];
}

static inline float readFloat32BigEndian(const uint8_t* in) {
  union {
    float f;
    uint32_t u;
  } conv;
  conv.u = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | (uint32_t)in[3];
  return conv.f;
}

static inline void writeFloat32BigEndian(float value, uint8_t* out) {
  union {
    float f;
    uint32_t u;
  } conv;
  conv.f = value;
  out[0] = (uint8_t)((conv.u >> 24) & 0xFF);
  out[1] = (uint8_t)((conv.u >> 16) & 0xFF);
  out[2] = (uint8_t)((conv.u >> 8) & 0xFF);
  out[3] = (uint8_t)(conv.u & 0xFF);
}

// 全局变量
BLEServer* pServer = nullptr;
BLECharacteristic* pButtonChar = nullptr;
BLECharacteristic* pVarDataChar = nullptr;
BLECharacteristic* pVarRequestChar = nullptr;
BLECharacteristic* pGpsDataChar = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint16_t lastButtonMask = 0;

// 批量请求相关变量
#define MAX_BATCH_VARS 16
int32_t pendingVarHashes[MAX_BATCH_VARS];
uint8_t pendingVarCount = 0;
uint8_t pendingVarIndex = 0;
uint32_t lastVarRequestTime = 0;
uint32_t lastBleNotifyTime = 0;

#define VAR_RESPONSE_SIZE 8
uint8_t batchResponseBuffer[MAX_BATCH_VARS * VAR_RESPONSE_SIZE];
uint8_t batchResponseCount = 0;

// 统计信息
uint32_t canTxCount = 0;
uint32_t canRxCount = 0;
uint32_t bleNotifyCount = 0;
uint32_t timeoutCount = 0;

// 硬件状态变量
uint32_t lastAdc1SampleTime = 0;
uint16_t adc1FilterBuffer[ADC1_FILTER_SAMPLES];
uint8_t adc1FilterIndex = 0;
bool adc1FilterFilled = false;
float lastAdc1Value = -1;

uint32_t lastDigitalSampleTime = 0;
uint16_t currentDigitalBits = 0;
uint16_t lastSentDigitalBits = 0xFFFF;
uint32_t lastDigitalChangeTime = 0;
bool digitalDebouncing = false;

// BLE服务器回调类 [7](@ref)
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    logMessage("BLE device connected");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    logMessage("BLE device disconnected");
  }
};

// 按钮特征回调类
class ButtonCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {

    String value = pCharacteristic->getValue();
    if (value.length() >= 2) {
      uint16_t buttonMask = (uint8_t)value[0] | ((uint8_t)value[1] << 8);
      if (buttonMask != lastButtonMask) {
        lastButtonMask = buttonMask;
        sendButtonCanFrame(buttonMask);
      }
    } else if (value.length() == 1) {
      uint8_t buttonId = (uint8_t)value[0];
      uint16_t buttonMask = (1 << buttonId);
      if (buttonMask != lastButtonMask) {
        lastButtonMask = buttonMask;
        sendButtonCanFrame(buttonMask);
      }
    }
  }
};

// 变量请求回调类
class VarRequestCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    // 将返回的String转换为C风格字符串，再构造std::string
    String value = pCharacteristic->getValue();
    size_t len = value.length();

    if (len >= 4) {
      if (pendingVarCount > 0 && pendingVarIndex < pendingVarCount) {
        return;
      }

      pendingVarCount = 0;
      pendingVarIndex = 0;
      batchResponseCount = 0;

      for (size_t i = 0; i + 4 <= len && pendingVarCount < MAX_BATCH_VARS; i += 4) {
        pendingVarHashes[pendingVarCount++] = readInt32BigEndian((const uint8_t*)(value.c_str() + i));
      }

      if (pendingVarCount > 0) {
        lastVarRequestTime = millis();
        requestCanVariable(pendingVarHashes[0]);
      }
    }
  }
};

// 变量设置回调类
class VarSetCharCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) override {
    // 将返回的String转换为C风格字符串，再构造std::string
    String value = pCharacteristic->getValue();
    size_t len = value.length();

    if (len < 8) {
      logMessage("VarSet data too short!");
      return;
    }

    for (size_t i = 0; i + 8 <= len; i += 8) {
      const uint8_t* data = (const uint8_t*)value.c_str() + i;
      int32_t varHash = readInt32BigEndian(data);

      // 使用ACAN2515发送CAN消息 [1](@ref)
      CANMessage frame;
      frame.id = CAN_GPS_DATA_BASE + ECU_ID;
      frame.ext = false;
      frame.rtr = false;
      frame.len = 8;
      memcpy(frame.data, data, 8);

      bool ok = can.tryToSend(frame);

      // 调试日志（可选）
      /*
      Serial.printf("GPS data sent - Hash: 0x%08X, OK: %s\n", 
                   varHash, ok ? "Y" : "N");
      */
    }
  }
};

// 发送按钮CAN帧（使用ACAN2515）[1](@ref)
void sendButtonCanFrame(uint16_t buttonMask) {
  CANMessage frame;
  frame.id = CANBUS_BUTTONBOX_ADDRESS;
  frame.ext = false;
  frame.rtr = false;
  frame.len = 5;

  frame.data[0] = 0x5A;
  frame.data[1] = 0x00;
  frame.data[2] = TS_HW_BUTTONBOX1_CATEGORY;
  frame.data[3] = static_cast<uint8_t>((buttonMask >> 8) & 0xFF);
  frame.data[4] = static_cast<uint8_t>(buttonMask & 0xFF);

  if (can.tryToSend(frame)) {
    canTxCount++;
    Serial.printf("Button frame sent: 0x%04X\n", buttonMask);
  }
}

// 请求CAN变量
void requestCanVariable(int32_t varHash) {
  CANMessage frame;
  frame.id = CAN_VAR_REQUEST_BASE + ECU_ID;
  frame.ext = false;
  frame.rtr = false;
  frame.len = 4;

  writeInt32BigEndian(varHash, frame.data);

  if (can.tryToSend(frame)) {
    canTxCount++;
    Serial.printf("Variable request sent: 0x%08X\n", varHash);
  }
}

// 发送变量到ECU
void sendVariableToEcu(int32_t varHash, float value) {
  CANMessage frame;
  frame.id = CAN_GPS_DATA_BASE + ECU_ID;
  frame.ext = false;
  frame.rtr = false;
  frame.len = 8;

  writeInt32BigEndian(varHash, frame.data);
  writeFloat32BigEndian(value, frame.data + 4);

  if (can.tryToSend(frame)) {
    canTxCount++;
    Serial.printf("Variable sent to ECU: 0x%08X = %.2f\n", varHash, value);
  }
}

// 发送批量BLE响应
void sendBatchedBleResponse() {
  if (batchResponseCount == 0) return;
  if (!deviceConnected || pVarDataChar == nullptr) return;

  uint32_t now = millis();
  if (now - lastBleNotifyTime < BLE_NOTIFY_MIN_INTERVAL_MS) return;

  pVarDataChar->setValue(batchResponseBuffer, batchResponseCount * VAR_RESPONSE_SIZE);
  pVarDataChar->notify();
  lastBleNotifyTime = now;
  bleNotifyCount++;

  pendingVarCount = 0;
  pendingVarIndex = 0;
  batchResponseCount = 0;
}

// 检查变量请求超时
void checkVarRequestTimeout() {
  if (pendingVarCount == 0 || pendingVarIndex >= pendingVarCount) return;

  uint32_t now = millis();
  if (now - lastVarRequestTime >= VAR_REQUEST_TIMEOUT_MS) {
    timeoutCount++;

    pendingVarIndex++;

    if (pendingVarIndex < pendingVarCount) {
      lastVarRequestTime = now;
      requestCanVariable(pendingVarHashes[pendingVarIndex]);
    } else {
      sendBatchedBleResponse();
    }
  }
}

// 处理CAN接收（使用ACAN2515）[1](@ref)
void processCanRx() {
  CANMessage frame;
  uint8_t rxCount = 0;
  const uint8_t maxRxPerLoop = 10;

  while (rxCount < maxRxPerLoop && can.receive(frame)) {
    rxCount++;
    canRxCount++;

    Serial.print("Received CAN frame ID: 0x");
    Serial.print(frame.id, HEX);
    Serial.print(" Data: ");
    for (int i = 0; i < frame.len; i++) {
      Serial.print(frame.data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // 检查是否为ECU的变量响应
    if (frame.id == (CAN_VAR_RESPONSE_BASE + ECU_ID) && frame.len >= 8) {
      if (deviceConnected && pVarDataChar != nullptr && pendingVarCount > 0) {
        if (batchResponseCount < MAX_BATCH_VARS) {
          memcpy(batchResponseBuffer + (batchResponseCount * VAR_RESPONSE_SIZE), frame.data, 8);
          batchResponseCount++;
        }

        pendingVarIndex++;

        if (pendingVarIndex < pendingVarCount) {
          lastVarRequestTime = millis();
          requestCanVariable(pendingVarHashes[pendingVarIndex]);
        } else {
          sendBatchedBleResponse();
        }
      }
    }
  }
}

// 传统发送命令函数
void sendCMD(uint16_t buttonMask) {
  sendButtonCanFrame(buttonMask);
}

// 采样硬件ADC1
void sampleHardwareAdc1() {
  uint32_t now = millis();
  if (now - lastAdc1SampleTime < ADC1_SAMPLE_INTERVAL_MS) return;
  lastAdc1SampleTime = now;

  uint16_t rawValue = analogRead(ADC1_PIN);
  uint16_t scaledValue = rawValue >> 2;

  adc1FilterBuffer[adc1FilterIndex] = scaledValue;
  adc1FilterIndex = (adc1FilterIndex + 1) % ADC1_FILTER_SAMPLES;
  if (adc1FilterIndex == 0) adc1FilterFilled = true;

  uint32_t sum = 0;
  uint8_t count = adc1FilterFilled ? ADC1_FILTER_SAMPLES : adc1FilterIndex;
  if (count == 0) return;

  for (uint8_t i = 0; i < count; i++) {
    sum += adc1FilterBuffer[i];
  }
  float filteredValue = (float)sum / count;

  if (lastAdc1Value < 0 || abs(filteredValue - lastAdc1Value) >= ADC1_CHANGE_THRESHOLD) {
    lastAdc1Value = filteredValue;
    sendVariableToEcu(VAR_HASH_ADC[1], filteredValue);
  }
}

// 读取数字输入
void readDigitalInputs(uint16_t* bits) {
  *bits = 0;
  int state = digitalRead(DIGITAL_INPUT_PIN);
  if (state == LOW) {
    *bits |= (uint16_t)(1u << 0);
  }
}

// 采样数字输入
void sampleDigitalInputs() {
  uint32_t now = millis();
  if (now - lastDigitalSampleTime < DIGITAL_SAMPLE_INTERVAL_MS) return;
  lastDigitalSampleTime = now;

  uint16_t newBits;
  readDigitalInputs(&newBits);

  if (newBits != currentDigitalBits) {
    currentDigitalBits = newBits;
    lastDigitalChangeTime = now;
    digitalDebouncing = true;
  }

  if (digitalDebouncing && (now - lastDigitalChangeTime >= DIGITAL_DEBOUNCE_MS)) {
    digitalDebouncing = false;

    if (currentDigitalBits != lastSentDigitalBits) {
      lastSentDigitalBits = currentDigitalBits;
      sendVariableToEcu(VAR_HASH_D22_D37, (float)currentDigitalBits);
      logMessage(String("Digital: 0x") + String(currentDigitalBits, HEX));
    }
  }
}

// CAN初始化（使用ACAN2515）[1](@ref)
void setupCan() {
  Serial.println("Initializing CAN with ACAN2515...");

  SPI.begin();

  ACAN2515Settings settings(QUARTZ_FREQUENCY, 500UL * 1000UL);  // 500 kbps
  settings.mRequestedMode = ACAN2515Settings::NormalMode;

  const uint16_t errorCode = can.begin(settings, [] {
    can.isr();
  });

  if (errorCode == 0) {
    Serial.println("CAN initialized successfully at 500kbps");
    Serial.print("Actual bit rate: ");
    Serial.print(settings.actualBitRate());
    Serial.println(" bit/s");
  } else {
    Serial.print("CAN initialization error: 0x");
    Serial.println(errorCode, HEX);
  }
}

// BLE初始化 [7,8](@ref)
void setupBLE() {
  BLEDevice::init("ESP32 Dashboard");
  BLEDevice::setMTU(517);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(BLEUUID(SERVICE_UUID), 25);

  // 按钮特征
  pButtonChar = pService->createCharacteristic(
    CHAR_BUTTON_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR);
  pButtonChar->setCallbacks(new ButtonCharCallbacks());

  // 变量数据特征
  pVarDataChar = pService->createCharacteristic(
    CHAR_VAR_DATA_UUID,
    BLECharacteristic::PROPERTY_NOTIFY);
  pVarDataChar->addDescriptor(new BLE2902());

  // 变量请求特征
  pVarRequestChar = pService->createCharacteristic(
    CHAR_VAR_REQUEST_UUID,
    BLECharacteristic::PROPERTY_WRITE_NR);
  pVarRequestChar->setCallbacks(new VarRequestCharCallbacks());

  // GPS数据特征
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

  logMessage("BLE server started");
}

// 日志函数
void logMessage(const String& message) {
  Serial.println(String(millis()) + "ms: " + message);
}

// 主设置函数
void setup() {
  Serial.begin(921600);
  Serial.setDebugOutput(true);

  // 硬件引脚初始化
  pinMode(ADC1_PIN, INPUT);
  analogReadResolution(12);
  pinMode(DIGITAL_INPUT_PIN, INPUT_PULLUP);

  // 初始化子系统
  setupCan();
  setupBLE();

  logMessage("Setup complete - BLE Dashboard ready with ACAN2515");
}

// 主循环函数
void loop() {
  esp_task_wdt_reset();

  processCanRx();
  checkVarRequestTimeout();
  sampleHardwareAdc1();
  sampleDigitalInputs();

  // BLE连接状态处理
  if (!deviceConnected && oldDeviceConnected) {
    static uint32_t disconnectTime = 0;
    if (disconnectTime == 0) {
      disconnectTime = millis();
    } else if (millis() - disconnectTime >= RECONNECT_DELAY_MS) {
      pServer->startAdvertising();
      logMessage("BLE advertising restarted");
      oldDeviceConnected = deviceConnected;
      disconnectTime = 0;
    }
  } else {
    if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
      pendingVarCount = 0;
      pendingVarIndex = 0;
      batchResponseCount = 0;
    }
  }

  yield();
}