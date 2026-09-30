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
    private static let maxNameBytes = 64
    private static let asciiWhitespace = CharacterSet(charactersIn: " \t\r\n")

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

    private static func limitedToNameBytes(_ name: String) -> String {
        var limited = ""
        for character in name {
            let next = String(character).utf8.count
            if limited.utf8.count + next > maxNameBytes { break }
            limited.append(character)
        }
        return limited
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

    func storeEveryChange() {
        let limited = DeviceNameModel.limitedToNameBytes(name)
        guard limited == name else {
            name = limited
            return
        }
        dh_set_device_name(name.trimmingCharacters(in: DeviceNameModel.asciiWhitespace))
    }
}
