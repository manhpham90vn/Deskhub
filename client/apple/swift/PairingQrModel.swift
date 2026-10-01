import Foundation
import Observation

@MainActor @Observable
final class PairingQrModel {
    private(set) var invite = ""
    private(set) var modules: [[Bool]] = []
    private(set) var expiresAt: Date?
    private(set) var expired = false

    var shown: Bool { !invite.isEmpty }
    var open: Bool { shown || expired }

    func hide() {
        guard open else { return }
        invite = ""
        modules = []
        expiresAt = nil
        expired = false
        DeskhubClient.revokePairingInvite()
    }

    func renew(port: UInt16, bindIp: String) {
        DeskhubClient.revokePairingInvite()
        show(port: port, bindIp: bindIp)
    }

    func secondsLeft(at now: Date) -> Int64 {
        guard let expiresAt else { return 0 }
        return Int64(expiresAt.timeIntervalSince(now).rounded(.up))
    }

    func expire() {
        guard shown else { return }
        invite = ""
        modules = []
        expiresAt = nil
        expired = true
    }

    func show(port: UInt16, bindIp: String) {
        invite = DeskhubClient.pairingInvite(port: port, bindIp: bindIp)
        modules = DeskhubClient.qrModules(invite)
        expired = false
        expiresAt = invite.isEmpty
            ? nil
            : Date().addingTimeInterval(TimeInterval(DeskhubClient.pairingTokenTtlSeconds))
    }
}
