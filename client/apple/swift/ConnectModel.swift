import Foundation
import Observation

#if os(macOS)
    import AppKit
#else
    import UIKit
#endif

@MainActor @Observable
final class ConnectModel {
    private static let lastAddressKey = "lastAddress"
    private static let retiredPasscodeKey = "lastPasscode"

    private static var lastAddress: String {
        UserDefaults.standard.removeObject(forKey: retiredPasscodeKey)
        return UserDefaults.standard.string(forKey: lastAddressKey) ?? ""
    }

    private static var defaultDeviceName: String {
        #if os(macOS)
            Host.current().localizedName ?? ProcessInfo.processInfo.hostName
        #else
            UIDevice.current.name
        #endif
    }

    private static var initialDeviceName: String {
        let stored = DeskhubClient.buffered(96) { dh_device_name($0, $1) }
        return stored.isEmpty ? ConnectModel.defaultDeviceName : stored
    }

    var address: String = DeskhubClient.addressHost(ConnectModel.lastAddress) {
        didSet { if address != oldValue { authed = nil } }
    }

    var port: String = DeskhubClient.addressPortText(ConnectModel.lastAddress) {
        didSet { if port != oldValue { authed = nil } }
    }

    var deviceName: String = ConnectModel.initialDeviceName
    private(set) var isConnecting = false
    var connectError = ""
    private(set) var acceptedAddress = ""
    private(set) var authed: HostQuery?
    @ObservationIgnored private var attempt = 0

    var canOpenDesktop: Bool { !(authed?.sources.isEmpty ?? true) }
    var canOpenShell: Bool { authed?.caps.terminal ?? false }
    var canOpenFiles: Bool { authed?.caps.files ?? false }

    func saveDeviceName() {
        deviceName = deviceName.trimmingCharacters(in: .whitespacesAndNewlines)
        if deviceName.isEmpty { deviceName = ConnectModel.defaultDeviceName }
        dh_set_device_name(deviceName)
    }

    func acceptAddress() -> String? {
        acceptedAddress = ""
        guard !address.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return nil }
        let composed = DeskhubClient.composeAddress(address, portText: port)
        guard let accepted = DeskhubClient.normalizedAddress(composed) else {
            connectError = DeskhubClient.buffered(192) { dh_invalid_address_line(composed, $0, $1) }
                + " " + DeskhubClient.string(DHStrInvalidAddressHint)
            return nil
        }
        address = DeskhubClient.addressHost(accepted)
        port = DeskhubClient.addressPortText(accepted)
        acceptedAddress = accepted
        connectError = ""
        UserDefaults.standard.set(accepted, forKey: ConnectModel.lastAddressKey)
        return accepted
    }

    func connectAuth() async -> HostQuery? {
        attempt += 1
        let mine = attempt
        authed = nil
        guard let accepted = acceptAddress() else { return nil }
        isConnecting = true
        let found = await Task.detached {
            DeskhubClient.listSources(address: accepted)
        }.value
        guard mine == attempt else { return nil }
        isConnecting = false
        guard let found else {
            connectError = DeskhubClient.sourceQueryFailed(accepted)
            return nil
        }
        authed = found
        return found
    }

    func forgetHost() {
        attempt += 1
        isConnecting = false
        authed = nil
        connectError = ""
    }
}
