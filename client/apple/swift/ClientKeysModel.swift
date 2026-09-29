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

struct ClientKeyRow: Identifiable, Hashable {
    let name: String
    let fingerprint: String

    var id: String { name }
    var shortFingerprint: String { FingerprintText.short(fingerprint) }
}

@MainActor @Observable
final class ClientKeysModel {
    private static let keyCapacity = 64
    private static let publicKeyCapacity = Int(DH_PUBLIC_KEY_TEXT_CAP)

    private(set) var keys: [ClientKeyRow] = []
    private(set) var error = ""

    func refresh() {
        keys = DeskhubClient.ffiList(
            Self.keyCapacity, DHClientKey(),
            { dh_client_keys($0, $1) },
            { raw in
                ClientKeyRow(
                    name: DeskhubClient.cString(raw.name),
                    fingerprint: DeskhubClient.cString(raw.fingerprint)
                )
            }
        )
    }

    func publicKey(of key: ClientKeyRow) -> String {
        DeskhubClient.buffered(Self.publicKeyCapacity) { dh_client_public_key(key.name, $0, $1) }
    }

    func generate(name: String) {
        finish(dh_client_key_generate(Self.trimmed(name)))
    }

    func importKey(name: String, privateKey: String, passphrase: String) {
        finish(dh_client_key_import(Self.trimmed(name), privateKey, passphrase))
    }

    func reportUnreadable() {
        error = Self.errorText(DHClientKeyUnreadable)
    }

    private func finish(_ result: DHClientKeyError) {
        error = result == DHClientKeyOk ? "" : Self.errorText(result)
        refresh()
    }

    private static func errorText(_ result: DHClientKeyError) -> String {
        String(cString: dh_client_key_error_text(result))
    }

    private static func trimmed(_ text: String) -> String {
        text.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
