import Foundation
import Observation

enum FingerprintText {
    private static let prefix = "SHA256:"
    private static let shortLength = 12

    static func short(_ fingerprint: String) -> String {
        let body = fingerprint.hasPrefix(prefix)
            ? fingerprint.dropFirst(prefix.count)
            : Substring(fingerprint)
        return String(body.prefix(shortLength))
    }
}

struct SavedHostRow: Identifiable, Hashable {
    let alias: String
    let endpoint: String
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
                fingerprint: DeskhubClient.cString(raw.fingerprint)
            )
        }
    }

    private static func errorText(_ error: DHHostProfileError) -> String {
        String(cString: dh_host_profile_error_text(error))
    }
}
