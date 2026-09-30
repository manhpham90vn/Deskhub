import Foundation
import Observation

@MainActor @Observable
final class ConnectModel {
    private static let lastAddressKey = "lastAddress"
    private static let statusPollInterval = Duration.seconds(1)

    private static var lastAddress: String {
        UserDefaults.standard.string(forKey: lastAddressKey) ?? ""
    }

    var address: String = DeskhubClient.addressHost(ConnectModel.lastAddress) {
        didSet { if address != oldValue { authed = nil } }
    }

    var port: String = DeskhubClient.addressPortText(ConnectModel.lastAddress) {
        didSet { if port != oldValue { authed = nil } }
    }

    private(set) var isConnecting = false
    private(set) var waitingStatus = ""
    var connectError = ""
    private(set) var acceptedAddress = ""
    private(set) var authed: HostQuery?
    private(set) var pendingTrust: PendingHostTrust?
    private(set) var trustedHostsRevision = 0
    @ObservationIgnored private var attempt = 0

    var canOpenDesktop: Bool { !(authed?.sources.isEmpty ?? true) }
    var canOpenShell: Bool { authed?.caps.terminal ?? false }
    var canOpenFiles: Bool { authed?.caps.files ?? false }

    private func acceptAddress() -> String? {
        acceptedAddress = ""
        guard !address.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return nil }
        let composed = DeskhubClient.composeAddress(address, portText: port)
        guard let accepted = DeskhubClient.normalizedAddress(composed) else {
            connectError = DeskhubClient.buffered(192) { dh_invalid_address_line(composed, $0, $1) }
                + " " + DeskhubClient.string(DHStrInvalidAddressHint)
            return nil
        }
        remember(accepted)
        return accepted
    }

    func connectAuth() async -> HostQuery? {
        let typed = address.trimmingCharacters(in: .whitespacesAndNewlines)
        if DeskhubClient.isPairingInvite(typed) {
            return await connectAuth(invite: typed)
        }
        attempt += 1
        let mine = attempt
        authed = nil
        guard let accepted = acceptAddress() else { return nil }
        guard let outcome = await query(address: accepted, invite: "", attempt: mine) else {
            return nil
        }
        return finish(outcome, fallbackAddress: accepted)
    }

    func connectAuth(invite: String) async -> HostQuery? {
        attempt += 1
        let mine = attempt
        authed = nil
        acceptedAddress = ""
        let first = DeskhubClient.pairingInviteAddress(invite)
        guard !first.isEmpty else {
            connectError = DeskhubClient.string(DHStrInviteInvalid)
            return nil
        }
        guard let outcome = await query(address: "", invite: invite, attempt: mine) else {
            return nil
        }
        return finish(outcome, fallbackAddress: first)
    }

    func cancelConnect() {
        DeskhubClient.cancelSourceQuery()
        forgetHost()
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

    func forgetHost() {
        attempt += 1
        isConnecting = false
        waitingStatus = ""
        authed = nil
        pendingTrust = nil
        connectError = ""
    }

    private func query(address: String, invite: String, attempt mine: Int) async
        -> HostQueryOutcome?
    {
        isConnecting = true
        waitingStatus = ""
        let statusPoll = Task { await pollWaitingStatus() }
        let outcome = await Task.detached {
            DeskhubClient.listSources(address: address, invite: invite)
        }.value
        statusPoll.cancel()
        guard mine == attempt else { return nil }
        isConnecting = false
        waitingStatus = ""
        return outcome
    }

    private func pollWaitingStatus() async {
        while !Task.isCancelled {
            try? await Task.sleep(for: ConnectModel.statusPollInterval)
            guard !Task.isCancelled else { return }
            waitingStatus = DeskhubClient.sourceQueryStatus()
        }
    }

    private func finish(_ outcome: HostQueryOutcome, fallbackAddress: String) -> HostQuery? {
        let answered = outcome.answeredAddress.isEmpty ? fallbackAddress : outcome.answeredAddress
        guard let found = outcome.query else {
            reportQueryFailure(outcome, address: answered)
            return nil
        }
        remember(answered)
        authed = found
        return found
    }

    private func remember(_ accepted: String) {
        address = DeskhubClient.addressHost(accepted)
        port = DeskhubClient.addressPortText(accepted)
        acceptedAddress = accepted
        connectError = ""
        UserDefaults.standard.set(accepted, forKey: ConnectModel.lastAddressKey)
    }

    private func reportQueryFailure(_ outcome: HostQueryOutcome, address: String) {
        guard outcome.newHostKey.isEmpty else {
            pendingTrust = PendingHostTrust(
                address: address,
                fingerprint: outcome.newHostKey,
                prompt: DeskhubClient.trustNewHostPrompt(address, fingerprint: outcome.newHostKey)
            )
            return
        }
        if !outcome.failure.isEmpty {
            connectError = outcome.failure
            return
        }
        connectError = outcome.failed(as: DHSourceQueryInviteMismatch)
            ? DeskhubClient.string(DHStrInviteInvalid)
            : DeskhubClient.sourceQueryFailed(address)
    }
}
