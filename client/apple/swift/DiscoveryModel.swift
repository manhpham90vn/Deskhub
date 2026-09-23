import Foundation
import Observation

nonisolated enum DeskhubDiscovery {
    static var defaultPort: UInt16 { dh_default_port() }

    static var rescanSeconds: Int { Int(dh_scan_rescan_secs()) }

    static var configuredPort: UInt16 {
        let stored = dh_settings_load().port
        return stored >= 1 && stored <= 65535 ? UInt16(stored) : defaultPort
    }

    static func startScan(port: UInt16) {
        _ = dh_scan_start(port)
    }

    static func restartScan(port: UInt16) {
        _ = dh_scan_restart(port)
    }

    static func refreshStatus() {
        dh_status_refresh_now()
    }

    static var isScanning: Bool { dh_scan_state().running }

    static func scanStatus(port: UInt16) -> String {
        DeskhubClient.buffered(256) { dh_scan_status_text(port, $0, $1) }
    }

    static func deviceRows() -> [DeviceListRow] {
        DeskhubClient.ffiList(
            64, DHDeviceRow(),
            { dh_device_rows($0, $1) },
            { row in
                DeviceListRow(
                    addr: DeskhubClient.cString(row.addr),
                    passcode: DeskhubClient.cString(row.passcode),
                    origin: DeskhubClient.cString(row.origin),
                    status: DeskhubClient.cString(row.status),
                    ping: DeskhubClient.cString(row.ping),
                    lastConnected: DeskhubClient.cString(row.lastConnected),
                    online: row.known ? row.online : nil
                )
            }
        )
    }

    static func remember(address: String, passcode: String) {
        dh_recent_touch(address, passcode)
        dh_status_watch_recent()
    }

    static func passcode(for address: String) -> String {
        DeskhubClient.buffered(16) { dh_recent_passcode(address, $0, $1) }
    }

    static func watchRecent() {
        dh_status_watch_recent()
    }
}

@MainActor @Observable
final class DiscoveryModel {
    private static let pollInterval = Duration.seconds(1)
    private static let rescanTicks = DeskhubDiscovery.rescanSeconds

    var devices: [DeviceListRow] = []
    var scanStatus = ""

    private var pump: Task<Void, Never>?
    private var port = DeskhubDiscovery.configuredPort

    func usePort(_ value: UInt16) {
        guard value != port else { return }
        port = value
        rescanNow()
    }

    func rescanNow() {
        scanStatus = DeskhubClient.string(DHStrLanDevicesEmpty)
        Task.detached { [port] in DeskhubDiscovery.restartScan(port: port) }
    }

    func refreshStatus() {
        Task.detached { DeskhubDiscovery.refreshStatus() }
    }

    func start() {
        guard pump == nil else { return }
        pump = Task {
            await Task.detached { [port] in
                DeskhubDiscovery.watchRecent()
                DeskhubDiscovery.startScan(port: port)
            }.value

            var idleTicks = 0
            while !Task.isCancelled {
                let snapshot = await Task.detached { [port] in
                    (
                        devices: DeskhubDiscovery.deviceRows(),
                        status: DeskhubDiscovery.scanStatus(port: port),
                        scanning: DeskhubDiscovery.isScanning
                    )
                }.value

                devices = snapshot.devices
                scanStatus = snapshot.status

                if snapshot.scanning {
                    idleTicks = 0
                } else {
                    idleTicks += 1
                    if idleTicks >= DiscoveryModel.rescanTicks {
                        idleTicks = 0
                        await Task.detached { [port] in
                            DeskhubDiscovery.startScan(port: port)
                        }.value
                    }
                }

                try? await Task.sleep(for: DiscoveryModel.pollInterval)
            }
        }
    }

    func remember(address: String, passcode: String) async {
        await Task.detached { DeskhubDiscovery.remember(address: address, passcode: passcode) }
            .value
        devices = await Task.detached { DeskhubDiscovery.deviceRows() }.value
    }
}
