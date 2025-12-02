import SwiftUI

struct ButtonGridView: View {
    @EnvironmentObject var settingsManager: SettingsManager
    @ObservedObject var dashboardState: DashboardState
    var onButtonChanged: () -> Void
    
    private let accentColor = Color(hex: "FF6B00")
    private let bgSurface = Color(hex: "1E1E1E")
    private let buttonBorder = Color(hex: "3A3A3A")
    
    var body: some View {
        let columns = Array(repeating: GridItem(.flexible(), spacing: 6), count: settingsManager.buttonColumns)
        let buttonCount = settingsManager.buttonCount
        
        LazyVGrid(columns: columns, spacing: 6) {
            ForEach(0..<buttonCount, id: \.self) { index in
                DashboardButton(
                    config: settingsManager.getButton(index),
                    isPressed: isButtonPressed(index),
                    isToggled: dashboardState.toggleStates[index] ?? false,
                    onPress: { handleButtonPress(index) },
                    onRelease: { handleButtonRelease(index) }
                )
            }
        }
    }
    
    private func isButtonPressed(_ index: Int) -> Bool {
        (dashboardState.buttonMask & (1 << index)) != 0
    }
    
    private func handleButtonPress(_ index: Int) {
        let config = settingsManager.getButton(index)
        
        if config.isToggle {
            // Toggle mode: flip state and send press then release
            let newState = dashboardState.toggleButton(index)
            if newState {
                dashboardState.setButton(index, pressed: true)
                onButtonChanged()
                
                // Send release after short delay
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
                    dashboardState.setButton(index, pressed: false)
                    onButtonChanged()
                }
            } else {
                dashboardState.setButton(index, pressed: true)
                onButtonChanged()
                
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.05) {
                    dashboardState.setButton(index, pressed: false)
                    onButtonChanged()
                }
            }
        } else {
            // Momentary mode: press
            dashboardState.setButton(index, pressed: true)
            onButtonChanged()
        }
    }
    
    private func handleButtonRelease(_ index: Int) {
        let config = settingsManager.getButton(index)
        
        if !config.isToggle {
            // Momentary mode: release
            dashboardState.setButton(index, pressed: false)
            onButtonChanged()
        }
    }
}

struct DashboardButton: View {
    let config: ButtonConfig
    let isPressed: Bool
    let isToggled: Bool
    var onPress: () -> Void
    var onRelease: () -> Void
    
    private let accentColor = Color(hex: "FF6B00")
    private let bgSurface = Color(hex: "1E1E1E")
    private let buttonBorder = Color(hex: "3A3A3A")
    
    var body: some View {
        let isActive = config.isToggle ? isToggled : isPressed
        
        Text(config.label)
            .font(.system(size: 11, weight: .semibold))
            .foregroundColor(isActive ? .black : .white)
            .frame(maxWidth: .infinity)
            .frame(height: 52)
            .background(isActive ? accentColor : bgSurface)
            .cornerRadius(10)
            .overlay(
                RoundedRectangle(cornerRadius: 10)
                    .stroke(isActive ? accentColor : buttonBorder, lineWidth: 1)
            )
            .scaleEffect(isPressed ? 0.95 : 1.0)
            .animation(.easeInOut(duration: 0.1), value: isPressed)
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in
                        onPress()
                    }
                    .onEnded { _ in
                        onRelease()
                    }
            )
    }
}

#Preview {
    ButtonGridView(
        dashboardState: DashboardState(),
        onButtonChanged: {}
    )
    .environmentObject(SettingsManager())
    .padding()
    .background(Color.black)
}
