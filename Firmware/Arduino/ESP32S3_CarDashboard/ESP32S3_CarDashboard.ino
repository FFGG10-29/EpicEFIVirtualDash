// ============================================================================
// ESP32S3 Car Dashboard - 主程序文件
// ============================================================================
// 依赖的头文件
#include "project_config.h"
#include "can_manager.h"
#include "ble_manager.h"

// ============================================================================
// 全局变量定义
// ============================================================================
// ADC1相关变量
uint32_t lastAdc1SampleTime = 0;
uint16_t adc1FilterBuffer[ADC1_FILTER_SAMPLES];
uint8_t adc1FilterIndex = 0;
bool adc1FilterFilled = false;
float lastAdc1Value = -1.0f;

// 数字输入相关变量
uint32_t lastDigitalSampleTime = 0;
uint16_t currentDigitalBits = 0;
uint16_t lastSentDigitalBits = 0xFFFF;
uint32_t lastDigitalChangeTime = 0;
bool digitalDebouncing = false;

// ============================================================================
// CAN数据接收回调函数
// ============================================================================
void onCanDataReceived(const uint8_t* data, uint8_t len) {
  if (!bleManager.isConnected()) return;
  if (len < 8) return;

  // 将接收到的CAN响应数据添加到批量缓冲区
  if (bleManager.batchResponseCount < MAX_BATCH_VARS) {
    memcpy(bleManager.batchResponseBuffer + (bleManager.batchResponseCount * VAR_RESPONSE_SIZE),
           data, 8);
    bleManager.batchResponseCount++;
  }

  // 移动到下一个变量请求
  bleManager.pendingVarIndex++;

  if (bleManager.pendingVarIndex < bleManager.pendingVarCount) {
    // 请求下一个变量
    bleManager.lastVarRequestTime = millis();
    canManager.requestVariable(bleManager.pendingVarHashes[bleManager.pendingVarIndex]);
  } else {
    // 所有变量请求完成，发送批量响应
    bleManager.sendBatchResponse();
  }
}

// ============================================================================
// 硬件采样函数
// ============================================================================
// 采样硬件ADC1
void sampleHardwareAdc1() {
  uint32_t now = millis();
  if (now - lastAdc1SampleTime < ADC1_SAMPLE_INTERVAL_MS) return;
  lastAdc1SampleTime = now;

  // 读取原始ADC值（12位分辨率）
  uint16_t rawValue = analogRead(ADC1_PIN);
  uint16_t scaledValue = rawValue >> 2;  // 缩放到10位

  // 应用简单的移动平均滤波
  adc1FilterBuffer[adc1FilterIndex] = scaledValue;
  adc1FilterIndex = (adc1FilterIndex + 1) % ADC1_FILTER_SAMPLES;

  if (adc1FilterIndex == 0) {
    adc1FilterFilled = true;
  }

  // 计算滤波后的值
  uint32_t sum = 0;
  uint8_t sampleCount = adc1FilterFilled ? ADC1_FILTER_SAMPLES : adc1FilterIndex;

  if (sampleCount == 0) return;

  for (uint8_t i = 0; i < sampleCount; i++) {
    sum += adc1FilterBuffer[i];
  }

  float filteredValue = (float)sum / sampleCount;

  // 检查值是否有显著变化
  if (lastAdc1Value < 0 || abs(filteredValue - lastAdc1Value) >= ADC1_CHANGE_THRESHOLD) {
    lastAdc1Value = filteredValue;

    // 发送到ECU
    canManager.sendVariableToEcu(VAR_HASH_ADC[1], filteredValue);

    logMessage("ADC1: " + String(filteredValue, 1) + " units");
  }
}

// 读取数字输入状态
void readDigitalInputs(uint16_t* bits) {
  *bits = 0;

  // 读取DIGITAL_INPUT_PIN（IO1）状态
  int state = digitalRead(DIGITAL_INPUT_PIN);
  if (state == LOW) {
    *bits |= (uint16_t)(1u << 0);  // 设置位0
  }

  // 可以在这里添加更多数字输入的读取
  // 例如：if (digitalRead(PIN2) == LOW) *bits |= (1u << 1);
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

      // 发送到ECU
      canManager.sendVariableToEcu(VAR_HASH_D22_D37, (float)currentDigitalBits);

      logMessage("Digital Inputs: 0x" + String(currentDigitalBits, HEX));
    }
  }
}

// ============================================================================
// Arduino主函数
// ============================================================================
void setup() {
  // 初始化串口
  Serial.begin(921600);
  Serial.setDebugOutput(true);
  delay(1000);  // 等待串口稳定

  logMessage("========================================");
  logMessage("ESP32S3 Car Dashboard - Starting...");
  logMessage("========================================");

  // 初始化硬件引脚
  pinMode(ADC1_PIN, INPUT);
  analogReadResolution(12);  // 设置ADC分辨率为12位

  pinMode(DIGITAL_INPUT_PIN, INPUT_PULLUP);

  logMessage("Hardware pins initialized");

  // 初始化CAN管理器
  if (canManager.init()) {
    // 设置CAN数据接收回调
    canManager.setRxCallback(onCanDataReceived);
    logMessage("CAN Manager initialized successfully");
  } else {
    logMessage("ERROR: CAN Manager initialization failed!");
  }

  // 初始化BLE管理器
  if (bleManager.init()) {
    logMessage("BLE Manager initialized successfully");
  } else {
    logMessage("ERROR: BLE Manager initialization failed!");
  }

  logMessage("Setup complete - System is ready");
  logMessage("========================================");
}

void loop() {

  // 处理CAN接收
  canManager.processRx();

  // 更新BLE状态
  bleManager.update();

  // 采样硬件输入
  sampleHardwareAdc1();
  sampleDigitalInputs();

  // 让出CPU时间
  yield();
}