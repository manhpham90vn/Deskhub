import Foundation
import Observation

#if os(macOS)
    import AppKit
#else
    import UIKit
#endif

@MainActor @Observable
final class DeviceNameModel {
    private static let capacity = 96

    static var saved: String {
        DeskhubClient.buffered(capacity) { dh_device_name($0, $1) }
    }

    static var sessionName: String {
        let stored = saved
        return stored.isEmpty ? systemName : stored
    }

    private static var systemName: String {
        #if os(macOS)
            Host.current().localizedName ?? ProcessInfo.processInfo.hostName
        #else
            UIDevice.current.name
        #endif
    }

    var name: String = DeviceNameModel.saved
    let placeholder = DeviceNameModel.systemName

    func commit() {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        guard trimmed != DeviceNameModel.saved else {
            name = trimmed
            return
        }
        dh_set_device_name(trimmed)
        name = DeviceNameModel.saved
    }
}
