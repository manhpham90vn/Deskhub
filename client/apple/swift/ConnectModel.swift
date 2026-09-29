import Foundation
import Observation

@MainActor @Observable
final class ConnectModel {
    private static let lastAddressKey = "lastAddress"
    private static let retiredPasscodeKey = "lastPasscode"

    private static var lastAddress: String {
        UserDefaults.standard.removeObject(forKey: retiredPasscodeKey)
        return UserDefaults.standard.string(forKey: lastAddressKey) ?? ""
    }

    var address: String = DeskhubClient.addressHost(ConnectModel.lastAddress) {
        didSet { if address != oldValue { authed = nil } }
    }

    var port: String = DeskhubClient.addressPortText(ConnectModel.lastAddress) {
        didSet { if port != oldValue { authed = nil } }
    }

    private(set) var isConnecting = false
    var connectError = ""
    private(set) var acceptedAddress = ""
    private(set) var authed: HostQuery?
    private(set) var pendingTrust: PendingHostTrust?
    private(set) var trustedHostsRevision = 0
    @ObservationIgnored private var attempt = 0

    var canOpenDesktop: Bool { !(authed?.sources.isEmpty ?? true) }
    var canOpenShell: Bool { authed?.caps.terminal ?? false }
    var canOpenFiles: Bool { authed?.caps.files ?? false }

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
        let outcome = await Task.detached {
            DeskhubClient.listSources(address: accepted)
        }.value
        guard mine == attempt else { return nil }
        isConnecting = false
        guard let found = outcome.query else {
            reportQueryFailure(outcome, address: accepted)
            return nil
        }
        authed = found
        return found
    }

    func trustPendingHost() -> String? {
        guard let pending = pendingTrust else { return nil }
        pendingTrust = nil
        let result = dh_host_trust_new(pending.address, pending.fingerprint)
        guard result == DHHostProfileOk else {
            connectError = String(cString: dh_host_profile_error_text(result))
            return nil
        }
        trustedHostsRevision += 1
        return pending.address
    }

    func declinePendingHost() {
        pendingTrust = nil
    }

    func target(_ address: String) {
        self.address = DeskhubClient.addressHost(address)
        port = DeskhubClient.addressPortText(address)
    }

    private func reportQueryFailure(_ outcome: HostQueryOutcome, address accepted: String) {
        guard outcome.newHostKey.isEmpty else {
            pendingTrust = PendingHostTrust(
                address: accepted,
                fingerprint: outcome.newHostKey,
                prompt: DeskhubClient.trustNewHostPrompt(accepted, fingerprint: outcome.newHostKey)
            )
            return
        }
        connectError = outcome.failure.isEmpty
            ? DeskhubClient.sourceQueryFailed(accepted) : outcome.failure
    }

    func forgetHost() {
        attempt += 1
        isConnecting = false
        authed = nil
        pendingTrust = nil
        connectError = ""
    }
}
