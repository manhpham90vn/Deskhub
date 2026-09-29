import Foundation
import Observation

struct SavedHostRow: Identifiable, Hashable {
    let alias: String
    let endpoint: String
    let identity: String
    let fingerprint: String

    var id: String { alias }

    var shortKey: String { FingerprintText.short(fingerprint) }
}

@MainActor @Observable
final class SavedHostsModel {
    private static let profileCapacity = 128

    private(set) var hosts: [SavedHostRow] = []
    private(set) var listError = ""

    func refresh() {
        loadHosts()
    }

    func remove(_ host: SavedHostRow) {
        let result = dh_host_profile_remove(host.alias)
        refresh()
        if result != DHHostProfileOk {
            listError = Self.errorText(result)
        }
    }

    private func loadHosts() {
        var buf = [DHHostProfile](repeating: DHHostProfile(), count: Self.profileCapacity)
        let count = buf.withUnsafeMutableBufferPointer { ptr in
            dh_host_profiles(ptr.baseAddress, Int32(ptr.count))
        }
        guard count >= 0 else {
            hosts = []
            listError = Self.errorText(DHHostProfileStoreUnreadable)
            return
        }
        listError = ""
        hosts = buf.prefix(Int(count)).map { raw in
            SavedHostRow(
                alias: DeskhubClient.cString(raw.alias),
                endpoint: DeskhubClient.cString(raw.endpoint),
                identity: DeskhubClient.cString(raw.identity),
                fingerprint: DeskhubClient.cString(raw.fingerprint)
            )
        }
    }

    private static func errorText(_ error: DHHostProfileError) -> String {
        String(cString: dh_host_profile_error_text(error))
    }
}
