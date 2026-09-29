import Foundation
import Observation

@MainActor @Observable
final class SharingModel {
    static let extensionBundleId = "com.ios.deskhub.broadcast"

    private static let pollInterval = Duration.milliseconds(1000)

    var status = BroadcastStatus()
    var addresses: [LocalAddress] = []
    var bindIp = DeskhubClient.buffered(64) { dh_bind_ip($0, $1) }

    var screenStatusLine: String {
        let port = UInt16(dh_settings_load().port)
        if status.sharing {
            return DeskhubClient.buffered(320) {
                dh_sharing_status(port, false, true, false, false, $0, $1)
            }
        }
        return DeskhubClient.buffered(160) { dh_idle_host_status(port, $0, $1) }
    }

    var filesStatusLine: String {
        guard FilesHost.shared.receiving else { return "" }
        let port = UInt16(dh_settings_load().port)
        return DeskhubClient.buffered(320) {
            dh_sharing_status(port, false, false, false, true, $0, $1)
        }
    }

    func saveBindIp() {
        dh_set_bind_ip(bindIp)
    }

    func poll() async {
        while !Task.isCancelled {
            status = BroadcastStatus.load()
            addresses = LocalAddress.all()
            try? await Task.sleep(for: SharingModel.pollInterval)
        }
    }
}
