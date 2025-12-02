import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var settingsManager: SettingsManager
    @Environment(\.dismiss) var dismiss
    
    @State private var selectedButton: ButtonConfig?
    @State private var showButtonEditor = false
    
    private let accentColor = Color(hex: "FF6B00")
    private let bgDark = Color(hex: "121212")
    private let bgSurface = Color(hex: "1E1E1E")
    
    var body: some View {
        NavigationStack {
            ZStack {
                bgDark.ignoresSafeArea()
                
                ScrollView {
                    VStack(spacing: 16) {
                        // Speed Unit
                        settingsCard(title: "Speed Unit") {
                            Picker("Speed Unit", selection: $settingsManager.speedUnit) {
                                ForEach(SpeedUnit.allCases, id: \.self) { unit in
                                    Text(unit.rawValue).tag(unit)
                                }
                            }
                            .pickerStyle(.segmented)
                        }
                        
                        // Data Rate
                        settingsCard(title: "Data Rate") {
                            VStack(alignment: .leading, spacing: 8) {
                                HStack {
                                    Text("Polling Rate")
                                        .foregroundColor(.white)
                                    Spacer()
                                    Text("\(settingsManager.dataRateHz) Hz")
                                        .foregroundColor(accentColor)
                                }
                                
                                Slider(
                                    value: Binding(
                                        get: { Double(settingsManager.dataRateHz) },
                                        set: { settingsManager.dataRateHz = Int($0) }
                                    ),
                                    in: 1...60,
                                    step: 1
                                )
                                .tint(accentColor)
                                
                                HStack {
                                    Text("1 Hz")
                                        .font(.caption)
                                        .foregroundColor(.gray)
                                    Spacer()
                                    Text("60 Hz")
                                        .font(.caption)
                                        .foregroundColor(.gray)
                                }
                            }
                        }
                        
                        // Button Configuration
                        settingsCard(title: "Buttons") {
                            VStack(alignment: .leading, spacing: 12) {
                                // Button count
                                HStack {
                                    Text("Button Count")
                                        .foregroundColor(.white)
                                    Spacer()
                                    Stepper("\(settingsManager.buttonCount)", value: $settingsManager.buttonCount, in: 1...16)
                                        .labelsHidden()
                                    Text("\(settingsManager.buttonCount)")
                                        .foregroundColor(accentColor)
                                        .frame(width: 30)
                                }
                                
                                // Columns
                                HStack {
                                    Text("Columns")
                                        .foregroundColor(.white)
                                    Spacer()
                                    Stepper("\(settingsManager.buttonColumns)", value: $settingsManager.buttonColumns, in: 2...4)
                                        .labelsHidden()
                                    Text("\(settingsManager.buttonColumns)")
                                        .foregroundColor(accentColor)
                                        .frame(width: 30)
                                }
                                
                                Divider()
                                
                                Text("Tap a button to configure:")
                                    .font(.caption)
                                    .foregroundColor(.gray)
                                
                                // Button grid for editing
                                buttonConfigGrid
                            }
                        }
                        
                        // Speedometer Toggle
                        settingsCard(title: "GPS Speedometer") {
                            Toggle("Show GPS Speed Gauge", isOn: $settingsManager.showSpeedometer)
                                .tint(accentColor)
                        }
                    }
                    .padding()
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") {
                        dismiss()
                    }
                    .foregroundColor(accentColor)
                }
            }
            .sheet(isPresented: $showButtonEditor) {
                if let button = selectedButton {
                    ButtonEditorView(config: button) { updated in
                        settingsManager.updateButton(button.id, config: updated)
                    }
                }
            }
        }
    }
    
    private func settingsCard<Content: View>(title: String, @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title)
                .font(.headline)
                .foregroundColor(accentColor)
            
            content()
        }
        .padding()
        .background(bgSurface)
        .cornerRadius(12)
    }
    
    private var buttonConfigGrid: some View {
        let columns = Array(repeating: GridItem(.flexible(), spacing: 6), count: 4)
        
        return LazyVGrid(columns: columns, spacing: 6) {
            ForEach(0..<settingsManager.buttonCount, id: \.self) { index in
                let config = settingsManager.getButton(index)
                
                Button {
                    selectedButton = config
                    showButtonEditor = true
                } label: {
                    VStack(spacing: 2) {
                        Text(config.label)
                            .font(.system(size: 9, weight: .medium))
                            .lineLimit(1)
                        
                        if config.isToggle {
                            Image(systemName: "arrow.triangle.2.circlepath")
                                .font(.system(size: 8))
                        }
                    }
                    .foregroundColor(.white)
                    .frame(maxWidth: .infinity)
                    .frame(height: 40)
                    .background(Color(hex: "2A2A2A"))
                    .cornerRadius(8)
                }
            }
        }
    }
}

struct ButtonEditorView: View {
    @Environment(\.dismiss) var dismiss
    let config: ButtonConfig
    var onSave: (ButtonConfig) -> Void
    
    @State private var label: String
    @State private var isToggle: Bool
    
    private let accentColor = Color(hex: "FF6B00")
    private let bgDark = Color(hex: "121212")
    
    init(config: ButtonConfig, onSave: @escaping (ButtonConfig) -> Void) {
        self.config = config
        self.onSave = onSave
        _label = State(initialValue: config.label)
        _isToggle = State(initialValue: config.isToggle)
    }
    
    var body: some View {
        NavigationStack {
            ZStack {
                bgDark.ignoresSafeArea()
                
                Form {
                    Section("Button Label") {
                        TextField("Label", text: $label)
                    }
                    
                    Section("Button Mode") {
                        Toggle("Toggle Mode", isOn: $isToggle)
                            .tint(accentColor)
                        
                        Text(isToggle ? "Button stays on/off when tapped" : "Button is active only while pressed")
                            .font(.caption)
                            .foregroundColor(.gray)
                    }
                }
                .scrollContentBackground(.hidden)
            }
            .navigationTitle("Edit Button \(config.id + 1)")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button("Cancel") {
                        dismiss()
                    }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Save") {
                        let updated = ButtonConfig(id: config.id, label: label, isToggle: isToggle)
                        onSave(updated)
                        dismiss()
                    }
                    .foregroundColor(accentColor)
                }
            }
        }
    }
}

#Preview {
    SettingsView()
        .environmentObject(SettingsManager())
}
