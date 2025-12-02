import SwiftUI

struct GaugeView: View {
    let config: GaugeConfig
    let value: Float
    let isLarge: Bool
    
    private let accentColor = Color(hex: "FF6B00")
    private let bgSurface = Color(hex: "1E1E1E")
    
    var body: some View {
        VStack(spacing: isLarge ? 8 : 4) {
            // Value
            Text(formattedValue)
                .font(.system(size: isLarge ? 48 : 24, weight: .bold, design: .rounded))
                .foregroundColor(.white)
                .minimumScaleFactor(0.5)
                .lineLimit(1)
            
            // Unit
            Text(config.unit)
                .font(.system(size: isLarge ? 16 : 10, weight: .medium))
                .foregroundColor(accentColor)
            
            // Label
            Text(config.name)
                .font(.system(size: isLarge ? 12 : 8))
                .foregroundColor(.gray)
                .lineLimit(1)
        }
        .frame(maxWidth: .infinity)
        .frame(height: isLarge ? 140 : 80)
        .padding(isLarge ? 16 : 8)
        .background(bgSurface)
        .cornerRadius(16)
        .overlay(
            RoundedRectangle(cornerRadius: 16)
                .stroke(accentColor.opacity(0.3), lineWidth: 1)
        )
    }
    
    private var formattedValue: String {
        if config.isGpsSpeed {
            return String(format: "%.0f", value)
        }
        
        // Format based on value magnitude
        if abs(value) >= 100 {
            return String(format: "%.0f", value)
        } else if abs(value) >= 10 {
            return String(format: "%.1f", value)
        } else {
            return String(format: "%.2f", value)
        }
    }
}

#Preview {
    HStack {
        GaugeView(
            config: GaugeConfig(name: "GPS Speed", variableHash: 0, unit: "MPH", isGpsSpeed: true),
            value: 65,
            isLarge: true
        )
        GaugeView(
            config: GaugeConfig(name: "AFR", variableHash: 123, unit: ""),
            value: 14.7,
            isLarge: true
        )
    }
    .padding()
    .background(Color.black)
}
