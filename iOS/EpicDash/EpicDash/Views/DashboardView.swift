import SwiftUI

struct DashboardView: View {
    @EnvironmentObject var bleManager: BleManager
    @EnvironmentObject var settingsManager: SettingsManager
    @EnvironmentObject var locationManager: LocationManager
    
    @StateObject private var dashboardState = DashboardState()
    @State private var showSettings = false
    @State private var pollingTask: Task<Void, Never>?
    
    private let accentColor = Color(hex: "FF6B00")
    private let bgDark = Color(hex: "121212")
    private let bgSurface = Color(hex: "1E1E1E")
    
    var body: some View {
        NavigationStack {
            ZStack {
                bgDark.ignoresSafeArea()
                
                VStack(spacing: 12) {
                    // Top Row Gauges (2 large)
                    topGaugesSection
                    
                    // Secondary Gauges (4 smaller)
                    secondaryGaugesSection
                    
                    // Button Grid
                    ButtonGridView(
                        dashboardState: dashboardState,
                        onButtonChanged: sendButtonMask
                    )
                    
                    Spacer()
                    
                    // Log Panel
                    logPanel
                }
                .padding(12)
            }
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .principal) {
                    Text("EpicDash")
                        .font(.system(size: 22, weight: .bold, design: .default))
                        .foregroundColor(accentColor)
                }
                
                ToolbarItem(placement: .navigationBarTrailing) {
                    HStack(spacing: 8) {
                        // Connection status
                        Circle()
                            .fill(bleManager.isConnected ? Color.green : Color.red)
                            .frame(width: 10, height: 10)
                        
                        Text(bleManager.isConnected ? "Online" : "Offline")
                            .font(.caption)
                            .foregroundColor(.gray)
                        
                        // Connect button
                        Button {
                            if bleManager.isConnected {
                                bleManager.disconnect()
                            } else {
                                bleManager.startScan()
                            }
                        } label: {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                                .foregroundColor(accentColor)
                        }
                        
                        // Settings button
                        Button {
                            showSettings = true
                        } label: {
                            Image(systemName: "gearshape")
                                .foregroundColor(.gray)
                        }
                    }
                }
            }
            .sheet(isPresented: $showSettings) {
                SettingsView()
            }
            .onAppear {
                setupBleCallbacks()
                locationManager.startUpdating()
                startPolling()
            }
            .onDisappear {
                pollingTask?.cancel()
            }
        }
    }
    
    // MARK: - Top Gauges (2 large)
    private var topGaugesSection: some View {
        let topGauges = Array(settingsManager.gauges.prefix(2))
        
        return HStack(spacing: 8) {
            ForEach(topGauges) { gauge in
                GaugeView(
                    config: gauge,
                    value: gaugeValue(for: gauge),
                    isLarge: true
                )
            }
        }
    }
    
    // MARK: - Secondary Gauges (4 smaller)
    private var secondaryGaugesSection: some View {
        let secondaryGauges = Array(settingsManager.gauges.dropFirst(2).prefix(4))
        
        return LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 8), count: 4), spacing: 8) {
            ForEach(secondaryGauges) { gauge in
                GaugeView(
                    config: gauge,
                    value: gaugeValue(for: gauge),
                    isLarge: false
                )
            }
        }
    }
    
    // MARK: - Log Panel
    private var logPanel: some View {
        ScrollView {
            ScrollViewReader { proxy in
                VStack(alignment: .leading, spacing: 2) {
                    ForEach(Array(bleManager.logMessages.enumerated()), id: \.offset) { index, message in
                        Text(message)
                            .font(.system(size: 9, design: .monospaced))
                            .foregroundColor(.gray)
                            .id(index)
                    }
                }
                .onChange(of: bleManager.logMessages.count) { _ in
                    if let lastIndex = bleManager.logMessages.indices.last {
                        proxy.scrollTo(lastIndex, anchor: .bottom)
                    }
                }
            }
        }
        .frame(height: 60)
        .padding(10)
        .background(bgSurface)
        .cornerRadius(12)
        .overlay(
            RoundedRectangle(cornerRadius: 12)
                .stroke(Color.gray.opacity(0.3), lineWidth: 1)
        )
    }
    
    // MARK: - Helpers
    private func gaugeValue(for gauge: GaugeConfig) -> Float {
        if gauge.isGpsSpeed {
            return Float(locationManager.getSpeed(in: settingsManager.speedUnit))
        }
        return dashboardState.gaugeValues[gauge.variableHash] ?? 0
    }
    
    private func setupBleCallbacks() {
        bleManager.onVariableData = { hash, value in
            dashboardState.updateGaugeValue(hash: hash, value: value)
        }
    }
    
    private func sendButtonMask() {
        bleManager.sendButtonMask(dashboardState.buttonMask)
    }
    
    private func startPolling() {
        pollingTask?.cancel()
        pollingTask = Task {
            while !Task.isCancelled {
                if bleManager.isConnected {
                    // Get non-GPS gauge hashes
                    let hashes = settingsManager.gauges
                        .filter { !$0.isGpsSpeed }
                        .map { $0.variableHash }
                    
                    if !hashes.isEmpty {
                        bleManager.requestVariablesBatch(hashes)
                    }
                }
                
                try? await Task.sleep(nanoseconds: UInt64(settingsManager.dataDelayMs) * 1_000_000)
            }
        }
    }
}

#Preview {
    DashboardView()
        .environmentObject(BleManager())
        .environmentObject(SettingsManager())
        .environmentObject(LocationManager())
}
