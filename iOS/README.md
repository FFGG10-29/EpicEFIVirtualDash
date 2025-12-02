# EpicDash iOS App

iOS version of the EpicDash virtual dashboard for EpicEFI ECUs.

## Features

- **Real-Time Gauges** - GPS speed + ECU variables (up to 60 Hz)
- **Customizable Buttons** - 1-16 buttons, momentary or toggle mode
- **Two-Tier Layout** - 2 large + 4 small gauges
- **Dark Theme** - Automotive-style UI with orange accents
- **SwiftUI** - Modern declarative UI framework

## Requirements

- iOS 16.0+
- Xcode 15.0+
- iPhone or iPad with Bluetooth LE

## Building

1. Open `EpicDash.xcodeproj` in Xcode
2. Select your development team in Signing & Capabilities
3. Build and run on device (BLE requires physical device)

## Architecture

```
EpicDash/
├── EpicDashApp.swift      # App entry point
├── ContentView.swift      # Root view with splash/disclaimer logic
├── Views/
│   ├── SplashView.swift       # Splash screen
│   ├── DashboardView.swift    # Main dashboard
│   ├── SettingsView.swift     # Settings screen
│   ├── GaugeView.swift        # Gauge component
│   └── ButtonGridView.swift   # Button grid component
├── Managers/
│   ├── BleManager.swift       # CoreBluetooth BLE handling
│   ├── SettingsManager.swift  # UserDefaults persistence
│   └── LocationManager.swift  # CoreLocation GPS speed
└── Models/
    └── Models.swift           # Data models
```

## BLE Protocol

Same as Android app - see root [README.md](../README.md) for protocol details.

### Service UUID
`4fafc201-1fb5-459e-8fcc-c5c9c331914b`

### Characteristics
| UUID | Name | Direction |
|------|------|-----------|
| `...a8` | Button | iOS → ESP32 |
| `...a9` | VarData | ESP32 → iOS |
| `...aa` | VarRequest | iOS → ESP32 |

## Permissions

Add to `Info.plist`:
- `NSBluetoothAlwaysUsageDescription` - BLE connection
- `NSLocationWhenInUseUsageDescription` - GPS speedometer
- `UIBackgroundModes` → `bluetooth-central` - Background BLE

## Customization

### Colors
Edit `Color(hex:)` values in view files:
- `accentColor`: `FF6B00` (orange)
- `bgDark`: `121212`
- `bgSurface`: `1E1E1E`

### Adding Gauges
Edit `VariableRepository.commonGauges` in `Models.swift`

## App Store Deployment

1. Create App Store Connect record
2. Configure signing in Xcode
3. Archive and upload via Xcode Organizer
4. Submit for review

## License

See root [LICENSE](../LICENSE) file.
