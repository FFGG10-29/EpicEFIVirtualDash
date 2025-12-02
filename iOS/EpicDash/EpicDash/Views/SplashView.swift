import SwiftUI

struct SplashView: View {
    @State private var logoOpacity = 0.0
    @State private var titleOpacity = 0.0
    @State private var taglineOpacity = 0.0
    
    var body: some View {
        ZStack {
            Color.black
                .ignoresSafeArea()
            
            VStack(spacing: 20) {
                // Logo
                Image("logo_epicdash")
                    .resizable()
                    .scaledToFit()
                    .frame(width: 240, height: 80)
                    .foregroundColor(.white)
                    .opacity(logoOpacity)
                
                // App Name
                Text("EpicDash")
                    .font(.system(size: 36, weight: .bold, design: .default))
                    .foregroundColor(Color(hex: "FF6B00"))
                    .opacity(titleOpacity)
                
                // Tagline
                Text("Virtual Dashboard for EpicEFI")
                    .font(.system(size: 14))
                    .foregroundColor(.gray)
                    .opacity(taglineOpacity)
            }
        }
        .onAppear {
            withAnimation(.easeIn(duration: 0.8)) {
                logoOpacity = 1.0
            }
            withAnimation(.easeIn(duration: 0.8).delay(0.4)) {
                titleOpacity = 1.0
            }
            withAnimation(.easeIn(duration: 0.8).delay(0.6)) {
                taglineOpacity = 1.0
            }
        }
    }
}

// MARK: - Color Extension
extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3: // RGB (12-bit)
            (a, r, g, b) = (255, (int >> 8) * 17, (int >> 4 & 0xF) * 17, (int & 0xF) * 17)
        case 6: // RGB (24-bit)
            (a, r, g, b) = (255, int >> 16, int >> 8 & 0xFF, int & 0xFF)
        case 8: // ARGB (32-bit)
            (a, r, g, b) = (int >> 24, int >> 16 & 0xFF, int >> 8 & 0xFF, int & 0xFF)
        default:
            (a, r, g, b) = (1, 1, 1, 0)
        }
        self.init(
            .sRGB,
            red: Double(r) / 255,
            green: Double(g) / 255,
            blue: Double(b) / 255,
            opacity: Double(a) / 255
        )
    }
}

#Preview {
    SplashView()
}
