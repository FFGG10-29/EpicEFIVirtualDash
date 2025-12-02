import SwiftUI

@main
struct EpicDashApp: App {
    @StateObject private var bleManager = BleManager()
    @StateObject private var settingsManager = SettingsManager()
    @StateObject private var locationManager = LocationManager()
    
    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bleManager)
                .environmentObject(settingsManager)
                .environmentObject(locationManager)
                .preferredColorScheme(.dark)
        }
    }
}
