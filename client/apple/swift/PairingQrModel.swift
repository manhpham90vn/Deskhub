import Foundation
import Observation

@MainActor @Observable
final class PairingQrModel {
    private(set) var invite = ""
    private(set) var modules: [[Bool]] = []

    var shown: Bool { !invite.isEmpty }

    func toggle(port: UInt16, bindIp: String) {
        if shown {
            hide()
        } else {
            show(port: port, bindIp: bindIp)
        }
    }

    func hide() {
        guard shown else { return }
        invite = ""
        modules = []
        DeskhubClient.revokePairingInvite()
    }

    private func show(port: UInt16, bindIp: String) {
        invite = DeskhubClient.pairingInvite(port: port, bindIp: bindIp)
        modules = DeskhubClient.qrModules(invite)
    }
}
