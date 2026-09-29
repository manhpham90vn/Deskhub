import Foundation
import Observation

nonisolated enum RecentDevices {
    static func rows() -> [DeviceListRow] {
        DeskhubClient.ffiList(
            64, DHDeviceRow(),
            { dh_device_rows($0, $1) },
            { row in
                DeviceListRow(
                    addr: DeskhubClient.cString(row.addr),
                    name: DeskhubClient.cString(row.name),
                    lastConnected: DeskhubClient.cString(row.lastConnected)
                )
            }
        )
    }
}

@MainActor @Observable
final class RecentDevicesModel {
    var devices: [DeviceListRow] = []

    func refresh() {
        Task { devices = await Task.detached { RecentDevices.rows() }.value }
    }

    func reload() async {
        devices = await Task.detached { RecentDevices.rows() }.value
    }
}
