import Foundation
import CoreBluetooth
import Combine

class BleManager: NSObject, ObservableObject {
    // MARK: - UUIDs (must match ESP32 firmware)
    static let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    static let buttonCharUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    static let varDataCharUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a9")
    static let varRequestCharUUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26aa")
    
    private static let deviceName = "ESP32 Dashboard"
    
    // MARK: - Published State
    @Published var isConnected = false
    @Published var isScanning = false
    @Published var logMessages: [String] = []
    
    // MARK: - Callbacks
    var onVariableData: ((Int, Float) -> Void)?
    
    // MARK: - Private
    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var buttonCharacteristic: CBCharacteristic?
    private var varDataCharacteristic: CBCharacteristic?
    private var varRequestCharacteristic: CBCharacteristic?
    
    private var buttonWriteQueue: [Data] = []
    private var varRequestQueue: [Data] = []
    private var isWritingButton = false
    private var isWritingVarRequest = false
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    // MARK: - Public Methods
    func startScan() {
        guard centralManager.state == .poweredOn else {
            log("Bluetooth not ready")
            return
        }
        
        guard !isScanning else {
            log("Already scanning")
            return
        }
        
        isScanning = true
        log("Starting BLE scan...")
        
        centralManager.scanForPeripherals(
            withServices: [Self.serviceUUID],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
        
        // Stop scan after 10 seconds
        DispatchQueue.main.asyncAfter(deadline: .now() + 10) { [weak self] in
            self?.stopScan()
        }
    }
    
    func stopScan() {
        guard isScanning else { return }
        isScanning = false
        centralManager.stopScan()
        log("Scan stopped")
    }
    
    func disconnect() {
        if let peripheral = peripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
        cleanup()
    }
    
    func sendButtonMask(_ mask: UInt16) {
        guard isConnected, buttonCharacteristic != nil else { return }
        
        var data = Data(count: 2)
        data[0] = UInt8(mask & 0xFF)
        data[1] = UInt8((mask >> 8) & 0xFF)
        
        buttonWriteQueue.append(data)
        processButtonQueue()
    }
    
    func requestVariable(_ varHash: Int) {
        guard isConnected, varRequestCharacteristic != nil else { return }
        
        var data = Data(count: 4)
        // Big-endian
        data[0] = UInt8((varHash >> 24) & 0xFF)
        data[1] = UInt8((varHash >> 16) & 0xFF)
        data[2] = UInt8((varHash >> 8) & 0xFF)
        data[3] = UInt8(varHash & 0xFF)
        
        varRequestQueue.append(data)
        processVarRequestQueue()
    }
    
    func requestVariablesBatch(_ varHashes: [Int]) {
        guard isConnected, varRequestCharacteristic != nil, !varHashes.isEmpty else { return }
        
        var data = Data(count: varHashes.count * 4)
        for (index, hash) in varHashes.enumerated() {
            let offset = index * 4
            data[offset] = UInt8((hash >> 24) & 0xFF)
            data[offset + 1] = UInt8((hash >> 16) & 0xFF)
            data[offset + 2] = UInt8((hash >> 8) & 0xFF)
            data[offset + 3] = UInt8(hash & 0xFF)
        }
        
        varRequestQueue.append(data)
        processVarRequestQueue()
    }
    
    // MARK: - Private Methods
    private func cleanup() {
        peripheral = nil
        buttonCharacteristic = nil
        varDataCharacteristic = nil
        varRequestCharacteristic = nil
        buttonWriteQueue.removeAll()
        varRequestQueue.removeAll()
        isWritingButton = false
        isWritingVarRequest = false
        
        DispatchQueue.main.async {
            self.isConnected = false
        }
    }
    
    private func processButtonQueue() {
        guard !isWritingButton, !buttonWriteQueue.isEmpty,
              let char = buttonCharacteristic, let peripheral = peripheral else { return }
        
        let data = buttonWriteQueue.removeFirst()
        isWritingButton = true
        peripheral.writeValue(data, for: char, type: .withoutResponse)
        
        // For withoutResponse, manually trigger next
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.01) { [weak self] in
            self?.isWritingButton = false
            self?.processButtonQueue()
        }
    }
    
    private func processVarRequestQueue() {
        guard !isWritingVarRequest, !varRequestQueue.isEmpty,
              let char = varRequestCharacteristic, let peripheral = peripheral else { return }
        
        let data = varRequestQueue.removeFirst()
        isWritingVarRequest = true
        peripheral.writeValue(data, for: char, type: .withoutResponse)
        
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.01) { [weak self] in
            self?.isWritingVarRequest = false
            self?.processVarRequestQueue()
        }
    }
    
    private func log(_ message: String) {
        print("[BLE] \(message)")
        DispatchQueue.main.async {
            self.logMessages.append(message)
            if self.logMessages.count > 50 {
                self.logMessages.removeFirst()
            }
        }
    }
}

// MARK: - CBCentralManagerDelegate
extension BleManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            log("Bluetooth powered on")
        case .poweredOff:
            log("Bluetooth powered off")
            cleanup()
        case .unauthorized:
            log("Bluetooth unauthorized")
        case .unsupported:
            log("Bluetooth unsupported")
        default:
            log("Bluetooth state: \(central.state.rawValue)")
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? "Unknown"
        
        if name == Self.deviceName || name.contains("ESP32") {
            log("Found device: \(name)")
            stopScan()
            
            self.peripheral = peripheral
            peripheral.delegate = self
            central.connect(peripheral, options: nil)
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log("Connected to \(peripheral.name ?? "device")")
        peripheral.discoverServices([Self.serviceUUID])
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        log("Disconnected")
        cleanup()
    }
    
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        log("Failed to connect: \(error?.localizedDescription ?? "unknown")")
        cleanup()
    }
}

// MARK: - CBPeripheralDelegate
extension BleManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        
        for service in services {
            if service.uuid == Self.serviceUUID {
                log("Found service")
                peripheral.discoverCharacteristics([
                    Self.buttonCharUUID,
                    Self.varDataCharUUID,
                    Self.varRequestCharUUID
                ], for: service)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        
        for char in characteristics {
            switch char.uuid {
            case Self.buttonCharUUID:
                buttonCharacteristic = char
                log("Found button characteristic")
            case Self.varDataCharUUID:
                varDataCharacteristic = char
                peripheral.setNotifyValue(true, for: char)
                log("Found variable data characteristic")
            case Self.varRequestCharUUID:
                varRequestCharacteristic = char
                log("Found variable request characteristic")
            default:
                break
            }
        }
        
        if buttonCharacteristic != nil {
            log("Ready")
            DispatchQueue.main.async {
                self.isConnected = true
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == Self.varDataCharUUID,
              let data = characteristic.value else { return }
        
        // Parse batched response: multiple 8-byte entries [hash(4) + value(4)]
        var offset = 0
        while offset + 8 <= data.count {
            let hash = Int(Int32(bigEndian: data.subdata(in: offset..<offset+4).withUnsafeBytes { $0.load(as: Int32.self) }))
            let value = data.subdata(in: offset+4..<offset+8).withUnsafeBytes { $0.load(as: UInt32.self) }
            let floatValue = Float(bitPattern: UInt32(bigEndian: value))
            
            DispatchQueue.main.async {
                self.onVariableData?(hash, floatValue)
            }
            offset += 8
        }
    }
}
