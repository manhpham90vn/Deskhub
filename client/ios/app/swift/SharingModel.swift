import Foundation
import Observation

@MainActor @Observable
final class SharingModel {
    static let extensionBundleId = "com.ios.deskhub.broadcast"

    private static let pollInterval = Duration.milliseconds(1000)

    var status = BroadcastStatus()
    var addresses: [LocalAddress] = []
    var bindIp = DeskhubClient.buffered(64) { dh_bind_ip($0, $1) }
    let qr = PairingQrModel()
    let accessRequests = AccessRequestsModel()
    private(set) var qrUnavailable = false

    var port: UInt16 { UInt16(dh_settings_load().port) }

    var screenStatusLine: String {
        if status.sharing {
            return DeskhubClient.buffered(320) {
                dh_sharing_status(port, false, true, false, false, $0, $1)
            }
        }
        return DeskhubClient.buffered(160) { dh_idle_host_status(port, $0, $1) }
    }

    var filesStatusLine: String {
        guard FilesHost.shared.receiving else { return "" }
        return DeskhubClient.buffered(320) {
            dh_sharing_status(port, false, false, false, true, $0, $1)
        }
    }

    var bindLabel: String {
        if bindIp.isEmpty { return DeskhubClient.string(DHStrBindAllInterfaces) }
        guard bindIpIsStale else { return bindIp }
        return "\(bindIp) (\(DeskhubClient.string(DHStrBindNotConnectedNote)))"
    }

    var shownAddresses: [LocalAddress] {
        addresses.filter { bindIp.isEmpty || $0.ip == bindIp }
    }

    var hostRowsLine: String {
        guard status.sharing else { return DeskhubClient.string(DHStrNotSharing) }
        guard status.viewers > 0 else { return DeskhubClient.string(DHStrNothingShared) }
        guard !status.viewerNames.isEmpty else { return "\(status.viewers)" }
        return "\(status.viewers): \(status.viewerNames)"
    }

    private var bindIpIsStale: Bool {
        !bindIp.isEmpty && !addresses.contains { $0.ip == bindIp }
    }

    func saveBindIp() {
        dh_set_bind_ip(bindIp)
    }

    func toggleQr() {
        if qr.open {
            qr.hide()
            qrUnavailable = false
            return
        }
        qr.show(port: port, bindIp: bindIp)
        qrUnavailable = !qr.shown
    }

    func renewQr() {
        qr.renew(port: port, bindIp: bindIp)
        qrUnavailable = !qr.shown
    }

    func poll() async {
        while !Task.isCancelled {
            status = BroadcastStatus.load()
            addresses = LocalAddress.all()
            if !status.sharing {
                qr.hide()
                qrUnavailable = false
            }
            if qr.shown, qr.secondsLeft(at: Date()) <= 0 { qr.expire() }
            accessRequests.refresh()
            try? await Task.sleep(for: SharingModel.pollInterval)
        }
    }
}
