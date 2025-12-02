import Foundation

// MARK: - Speed Unit
enum SpeedUnit: String, Codable, CaseIterable {
    case mph = "MPH"
    case kph = "KPH"
    
    var multiplier: Double {
        switch self {
        case .mph: return 2.23694  // m/s to mph
        case .kph: return 3.6      // m/s to kph
        }
    }
}

// MARK: - Button Configuration
struct ButtonConfig: Codable, Identifiable {
    var id: Int
    var label: String
    var isToggle: Bool
    
    init(id: Int, label: String? = nil, isToggle: Bool = false) {
        self.id = id
        self.label = label ?? "BTN \(id + 1)"
        self.isToggle = isToggle
    }
}

// MARK: - Gauge Configuration
struct GaugeConfig: Codable, Identifiable, Equatable {
    var id: UUID = UUID()
    var name: String
    var variableHash: Int
    var unit: String
    var minValue: Float
    var maxValue: Float
    var isGpsSpeed: Bool
    
    init(name: String, variableHash: Int, unit: String = "", minValue: Float = 0, maxValue: Float = 100, isGpsSpeed: Bool = false) {
        self.name = name
        self.variableHash = variableHash
        self.unit = unit
        self.minValue = minValue
        self.maxValue = maxValue
        self.isGpsSpeed = isGpsSpeed
    }
    
    static func == (lhs: GaugeConfig, rhs: GaugeConfig) -> Bool {
        lhs.variableHash == rhs.variableHash
    }
}

// MARK: - Variable Definition (from JSON)
struct VariableDefinition: Codable {
    let name: String
    let hash: Int
    let source: String
}

// MARK: - Variable Repository
class VariableRepository {
    static let shared = VariableRepository()
    
    // GPS Speed pseudo-gauge (hash = 0)
    static let gpsSpeedGauge = GaugeConfig(
        name: "GPS Speed",
        variableHash: 0,
        unit: "MPH",
        minValue: 0,
        maxValue: 200,
        isGpsSpeed: true
    )
    
    // Common gauges
    static let commonGauges: [GaugeConfig] = [
        GaugeConfig(name: "AFR", variableHash: -1412584499, unit: "", minValue: 10, maxValue: 20),
        GaugeConfig(name: "Baro", variableHash: 1986862668, unit: "kPa", minValue: 80, maxValue: 110),
        GaugeConfig(name: "Ignition", variableHash: -1803182614, unit: "°", minValue: -10, maxValue: 50),
        GaugeConfig(name: "Boost", variableHash: -1106583792, unit: "%", minValue: 0, maxValue: 100),
        GaugeConfig(name: "Coolant", variableHash: 123456789, unit: "°F", minValue: 100, maxValue: 250),
        GaugeConfig(name: "Oil Pres", variableHash: 987654321, unit: "PSI", minValue: 0, maxValue: 100),
    ]
    
    private(set) var variables: [VariableDefinition] = []
    
    func loadVariables() {
        guard let url = Bundle.main.url(forResource: "variables", withExtension: "json"),
              let data = try? Data(contentsOf: url),
              let decoded = try? JSONDecoder().decode([VariableDefinition].self, from: data) else {
            return
        }
        variables = decoded
    }
    
    func getVariable(byHash hash: Int) -> VariableDefinition? {
        variables.first { $0.hash == hash }
    }
}

// MARK: - Dashboard State
class DashboardState: ObservableObject {
    @Published var buttonMask: UInt16 = 0
    @Published var toggleStates: [Int: Bool] = [:]
    @Published var gaugeValues: [Int: Float] = [:]
    @Published var gpsSpeed: Double = 0
    
    func setButton(_ index: Int, pressed: Bool) {
        if pressed {
            buttonMask |= (1 << index)
        } else {
            buttonMask &= ~(1 << index)
        }
    }
    
    func toggleButton(_ index: Int) -> Bool {
        let newState = !(toggleStates[index] ?? false)
        toggleStates[index] = newState
        return newState
    }
    
    func updateGaugeValue(hash: Int, value: Float) {
        gaugeValues[hash] = value
    }
}
