#include "can_manager.h"
#include "ble_manager.h"

// 全局CAN管理器实例
CanManager canManager;

// 辅助函数定义（放在CPP文件中）
void writeInt32BigEndian(int32_t value, uint8_t* out) {
    out[0] = (uint8_t)((value >> 24) & 0xFF);
    out[1] = (uint8_t)((value >> 16) & 0xFF);
    out[2] = (uint8_t)((value >> 8) & 0xFF);
    out[3] = (uint8_t)(value & 0xFF);
}

int32_t readInt32BigEndian(const uint8_t* in) {
    return ((int32_t)in[0] << 24) | 
           ((int32_t)in[1] << 16) | 
           ((int32_t)in[2] << 8) | 
           (int32_t)in[3];
}

float readFloat32BigEndian(const uint8_t* in) {
    union {
        float f;
        uint32_t u;
    } converter;
    
    converter.u = ((uint32_t)in[0] << 24) | 
                  ((uint32_t)in[1] << 16) | 
                  ((uint32_t)in[2] << 8) | 
                  (uint32_t)in[3];
    
    return converter.f;
}

void writeFloat32BigEndian(float value, uint8_t* out) {
    union {
        float f;
        uint32_t u;
    } converter;
    
    converter.f = value;
    out[0] = (uint8_t)((converter.u >> 24) & 0xFF);
    out[1] = (uint8_t)((converter.u >> 16) & 0xFF);
    out[2] = (uint8_t)((converter.u >> 8) & 0xFF);
    out[3] = (uint8_t)(converter.u & 0xFF);
}

// ============================================================================
// CanManager 实现
// ============================================================================

CanManager::CanManager() : can(MCP2515_CS, SPI, MCP2515_INT) {}

bool CanManager::init() {
    Serial.println("CAN Manager: Initializing MCP2515...");
    
    SPI.begin();
    
    ACAN2515Settings settings(QUARTZ_FREQUENCY, 500UL * 1000UL);  // 500 kbps
    settings.mRequestedMode = ACAN2515Settings::NormalMode;
    
    const uint16_t errorCode = can.begin(settings, [] {
        canManager.can.isr();
    });
    
    if (errorCode == 0) {
        Serial.println("CAN Manager: Initialized successfully at 500kbps");
        return true;
    } else {
        Serial.print("CAN Manager: Initialization error 0x");
        Serial.println(errorCode, HEX);
        return false;
    }
}

void CanManager::processRx() {
    CANMessage frame;
    uint8_t rxCount = 0;
    const uint8_t maxRxPerLoop = 10;
    
    while (rxCount < maxRxPerLoop && can.receive(frame)) {
        rxCount++;
        canRxCount++;
        handleReceivedFrame(frame);
    }
}

void CanManager::handleReceivedFrame(const CANMessage& frame) {
    // 调试输出
    Serial.print("CAN RX - ID: 0x");
    Serial.print(frame.id, HEX);
    Serial.print(", Len: ");
    Serial.print(frame.len);
    Serial.print(", Data: ");
    for (int i = 0; i < frame.len; i++) {
        Serial.print(frame.data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    
    // 检查是否为ECU的变量响应
    if (frame.id == (CAN_VAR_RESPONSE_BASE + ECU_ID) && frame.len >= 8) {
        if (rxCallback) {
            rxCallback(frame.data, frame.len);
        }
    }
}

bool CanManager::sendButtonFrame(uint16_t buttonMask) {
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
        Serial.printf("CAN TX: Button frame 0x%04X sent\n", buttonMask);
        return true;
    }
    
    Serial.println("CAN TX: Failed to send button frame");
    return false;
}

bool CanManager::requestVariable(int32_t varHash) {
    CANMessage frame;
    frame.id = CAN_VAR_REQUEST_BASE + ECU_ID;
    frame.ext = false;
    frame.rtr = false;
    frame.len = 4;
    
    writeInt32BigEndian(varHash, frame.data);
    
    if (can.tryToSend(frame)) {
        canTxCount++;
        Serial.printf("CAN TX: Variable request 0x%08X sent\n", varHash);
        return true;
    }
    
    Serial.println("CAN TX: Failed to send variable request");
    return false;
}

bool CanManager::sendVariableToEcu(int32_t varHash, float value) {
    CANMessage frame;
    frame.id = CAN_GPS_DATA_BASE + ECU_ID;
    frame.ext = false;
    frame.rtr = false;
    frame.len = 8;
    
    writeInt32BigEndian(varHash, frame.data);
    writeFloat32BigEndian(value, frame.data + 4);
    
    if (can.tryToSend(frame)) {
        canTxCount++;
        Serial.printf("CAN TX: Variable 0x%08X = %.2f sent to ECU\n", varHash, value);
        return true;
    }
    
    Serial.println("CAN TX: Failed to send variable to ECU");
    return false;
}

void CanManager::setRxCallback(void (*callback)(const uint8_t* data, uint8_t len)) {
    rxCallback = callback;
}