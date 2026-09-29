import Foundation
import Observation

nonisolated enum DeskhubDiscovery {
    static var defaultPort: UInt16 { dh_default_port() }

    static var configuredPort: UInt16 {
        let stored = dh_settings_load().port
        return stored >= 1 && stored <= 65535 ? UInt16(stored) : defaultPort
    }

    static func deviceRows() -> [DeviceListRow] {
        DeskhubClient.ffiList(
            64, DHDeviceRow(),
            { dh_device_rows($0, $1) },
            { row in
                DeviceListRow(
                    addr: DeskhubClient.cString(row.addr),
                    origin: DeskhubClient.cString(row.origin),
                    status: DeskhubClient.cString(row.status),
                    ping: DeskhubClient.cString(row.ping),
                    lastConnected: DeskhubClient.cString(row.lastConnected),
                    online: row.known ? row.online : nil
                )
            }
        )
    }

    static func remember(address: String) {
        dh_recent_touch(address)
    }
}

@MainActor @Observable
final class DiscoveryModel {
    var devices: [DeviceListRow] = []
    var scanStatus = ""

    func usePort(_ value: UInt16) {
        _ = value
        rescanNow()
    }

    func rescanNow() {
        Task { devices = await Task.detached { DeskhubDiscovery.deviceRows() }.value }
    }

    func start() {
        rescanNow()
    }

    func remember(address: String) async {
        await Task.detached { DeskhubDiscovery.remember(address: address) }.value
        devices = await Task.detached { DeskhubDiscovery.deviceRows() }.value
    }
}
