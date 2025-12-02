import SwiftUI

struct ContentView: View {
    @EnvironmentObject var settingsManager: SettingsManager
    @State private var showSplash = true
    @State private var showDisclaimer = false
    @State private var showWhatsNew = false
    
    var body: some View {
        ZStack {
            if showSplash {
                SplashView()
                    .transition(.opacity)
            } else {
                DashboardView()
                    .transition(.opacity)
            }
        }
        .onAppear {
            DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
                withAnimation(.easeInOut(duration: 0.5)) {
                    checkDisclaimerAndProceed()
                }
            }
        }
        .alert("⚠️ Off-Road Use Only", isPresented: $showDisclaimer) {
            Button("I Understand & Accept") {
                settingsManager.disclaimerAccepted = true
                checkForUpdate()
            }
            Button("Decline", role: .cancel) {
                exit(0)
            }
        } message: {
            Text("""
            IMPORTANT DISCLAIMER

            EpicDash is designed for OFF-ROAD and COMPETITION USE ONLY.

            • This application is NOT intended for use on public roads
            • Do not operate this device while driving
            • The driver should never interact with this application while the vehicle is in motion
            • Always follow local laws and regulations
            • Use at your own risk

            By continuing, you acknowledge that you understand and accept these terms.
            """)
        }
        .alert("🆕 What's New in v\(Bundle.main.appVersion)", isPresented: $showWhatsNew) {
            Button("Got it!") {
                settingsManager.lastVersionCode = Bundle.main.buildNumber
                showSplash = false
            }
        } message: {
            Text(loadWhatsNewText())
        }
    }
    
    private func checkDisclaimerAndProceed() {
        if settingsManager.disclaimerAccepted {
            checkForUpdate()
        } else {
            showDisclaimer = true
        }
    }
    
    private func checkForUpdate() {
        let lastVersion = settingsManager.lastVersionCode
        let currentVersion = Bundle.main.buildNumber
        
        if lastVersion > 0 && currentVersion > lastVersion {
            showWhatsNew = true
        } else {
            settingsManager.lastVersionCode = currentVersion
            showSplash = false
        }
    }
    
    private func loadWhatsNewText() -> String {
        guard let path = Bundle.main.path(forResource: "whatsnew", ofType: "txt"),
              let content = try? String(contentsOfFile: path, encoding: .utf8) else {
            return "• Bug fixes and performance improvements"
        }
        return content
    }
}

extension Bundle {
    var appVersion: String {
        infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0"
    }
    
    var buildNumber: Int {
        Int(infoDictionary?["CFBundleVersion"] as? String ?? "0") ?? 0
    }
}

#Preview {
    ContentView()
        .environmentObject(BleManager())
        .environmentObject(SettingsManager())
        .environmentObject(LocationManager())
}
