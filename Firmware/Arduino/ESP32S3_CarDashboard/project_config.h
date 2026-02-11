#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include <Arduino.h>

// ============================================================================
// 调试配置
// ============================================================================
#ifdef CORE_DEBUG_LEVEL
#undef CORE_DEBUG_LEVEL
#endif
#define CORE_DEBUG_LEVEL 0

// ============================================================================
// 硬件引脚定义
// ============================================================================
#define RGB_PIN 38           // WS2812 data pin
#define ADC1_PIN 5           // Hardware ADC input for ADC1 (GPIO 5)
#define DIGITAL_INPUT_PIN 1  // IO1 - touch sensor

// MCP2515引脚配置
static const byte MCP2515_CS = 10;
static const byte MCP2515_INT = 3;
static const uint32_t QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL;  // 8MHz晶振

// ============================================================================
// 硬件配置
// ============================================================================
#define ADC1_SAMPLE_INTERVAL_MS 10  // Sample every 10ms (100 Hz)
#define ADC1_FILTER_SAMPLES 1       // Number of samples for averaging filter (1 = no filter)
#define ADC1_CHANGE_THRESHOLD 5     // Minimum change to trigger CAN send


#define DIGITAL_SAMPLE_INTERVAL_MS 20  // Sample every 20ms (50 Hz)
#define DIGITAL_DEBOUNCE_MS 50         // Debounce time


// ============================================================================
// 超时配置
// ============================================================================
#define VAR_REQUEST_TIMEOUT_MS 100     // Timeout waiting for ECU response
#define BLE_NOTIFY_MIN_INTERVAL_MS 10  // Minimum interval between BLE notifications
#define RECONNECT_DELAY_MS 100         // Delay before restarting advertising (was 500)

// ============================================================================
// CAN协议定义
// ============================================================================
#define TS_HW_BUTTONBOX1_CATEGORY 27    // hardware button box 1
#define CANBUS_BUTTONBOX_ADDRESS 0x711  // CANBUS BUTTONBOX TX
#define ECU_ID 1
#define CAN_VAR_REQUEST_BASE 0x700   // TX: Request variable (0x700 + ecuId)
#define CAN_VAR_RESPONSE_BASE 0x720  // RX: Variable broadcast (0x720 + ecuId)
#define CAN_GPS_DATA_BASE 0x780      // TX: GPS data to ECU (0x780 + ecuId)

// ============================================================================
// BLE UUID定义
// ============================================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_BUTTON_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_VAR_DATA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_VAR_REQUEST_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_GPS_DATA_UUID "beb5483e-36e1-4688-b7f5-ea07361b26ab"

// ============================================================================
// 批量请求配置
// ============================================================================
#define MAX_BATCH_VARS 16  // 4 bytes hash + 4 bytes value
#define VAR_RESPONSE_SIZE 8

// ============================================================================
// 变量哈希定义
// ============================================================================
// GPS变量哈希
const int32_t VAR_HASH_GPS_HMSD_PACKED = 703958849;      // Hours, minutes, seconds, days (packed)
const int32_t VAR_HASH_GPS_MYQSAT_PACKED = -1519914092;  // Months, years, quality, satellites (packed)
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

// 数字输入变量哈希
const int32_t VAR_HASH_D22_D37 = 2138825443;

// ============================================================================
// 辅助函数声明
// ============================================================================
void writeInt32BigEndian(int32_t value, uint8_t* out);
int32_t readInt32BigEndian(const uint8_t* in);
float readFloat32BigEndian(const uint8_t* in);
void writeFloat32BigEndian(float value, uint8_t* out);
void logMessage(const String& message);

#endif  // PROJECT_CONFIG_H