import Foundation
import CoreLocation
import Combine

class LocationManager: NSObject, ObservableObject {
    @Published var speed: Double = 0  // Speed in m/s
    @Published var latitude: Double = 0
    @Published var longitude: Double = 0
    @Published var altitude: Double = 0
    @Published var course: Double = 0
    @Published var accuracy: Double = 0
    @Published var isAuthorized = false
    
    // Callback for sending GPS data to BLE
    var onGpsUpdate: ((CLLocation) -> Void)?
    
    // Track last values to only send on change
    private var lastSpeed: Float = -1
    private var lastLatitude: Double = -999
    private var lastLongitude: Double = -999
    private var lastAltitude: Float = -9999
    private var lastCourse: Float = -1
    private var lastAccuracy: Float = -1
    
    private let locationManager = CLLocationManager()
    
    override init() {
        super.init()
        locationManager.delegate = self
        locationManager.desiredAccuracy = kCLLocationAccuracyBestForNavigation
        locationManager.distanceFilter = kCLDistanceFilterNone  // No filter for max updates
        locationManager.activityType = .automotiveNavigation
    }
    
    func requestPermission() {
        locationManager.requestWhenInUseAuthorization()
    }
    
    func startUpdating() {
        guard isAuthorized else {
            requestPermission()
            return
        }
        locationManager.startUpdatingLocation()
        print("[Location] Started high-precision GPS updates")
    }
    
    func stopUpdating() {
        locationManager.stopUpdatingLocation()
    }
    
    func getSpeed(in unit: SpeedUnit) -> Double {
        max(0, speed * unit.multiplier)
    }
}

extension LocationManager: CLLocationManagerDelegate {
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        switch manager.authorizationStatus {
        case .authorizedWhenInUse, .authorizedAlways:
            isAuthorized = true
            startUpdating()
        case .denied, .restricted:
            isAuthorized = false
        case .notDetermined:
            requestPermission()
        @unknown default:
            break
        }
    }
    
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let location = locations.last else { return }
        
        DispatchQueue.main.async {
            // Update published properties
            if location.speed >= 0 {
                self.speed = location.speed
            }
            self.latitude = location.coordinate.latitude
            self.longitude = location.coordinate.longitude
            self.altitude = location.altitude
            self.course = location.course >= 0 ? location.course : 0
            self.accuracy = location.horizontalAccuracy
            
            // Notify callback for BLE transmission
            self.onGpsUpdate?(location)
        }
    }
    
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        print("[Location] Error: \(error.localizedDescription)")
    }
    
    // Build GPS entries for BLE transmission (only changed values)
    func buildGpsEntries(from location: CLLocation) -> [(hash: Int32, value: Float)] {
        var entries: [(hash: Int32, value: Float)] = []
        
        // Speed (m/s)
        let speedMs = Float(max(0, location.speed))
        if speedMs != lastSpeed {
            lastSpeed = speedMs
            entries.append((BleManager.VAR_HASH_GPS_SPEED, speedMs))
        }
        
        // Latitude
        if location.coordinate.latitude != lastLatitude {
            lastLatitude = location.coordinate.latitude
            entries.append((BleManager.VAR_HASH_GPS_LATITUDE, Float(location.coordinate.latitude)))
        }
        
        // Longitude
        if location.coordinate.longitude != lastLongitude {
            lastLongitude = location.coordinate.longitude
            entries.append((BleManager.VAR_HASH_GPS_LONGITUDE, Float(location.coordinate.longitude)))
        }
        
        // Altitude
        let alt = Float(location.altitude)
        if alt != lastAltitude {
            lastAltitude = alt
            entries.append((BleManager.VAR_HASH_GPS_ALTITUDE, alt))
        }
        
        // Course
        if location.course >= 0 {
            let courseVal = Float(location.course)
            if courseVal != lastCourse {
                lastCourse = courseVal
                entries.append((BleManager.VAR_HASH_GPS_COURSE, courseVal))
            }
        }
        
        // Accuracy
        let acc = Float(location.horizontalAccuracy)
        if acc != lastAccuracy {
            lastAccuracy = acc
            entries.append((BleManager.VAR_HASH_GPS_ACCURACY, acc))
        }
        
        return entries
    }
}
