import Observation
import SwiftUI

struct PairingAsk: Identifiable {
    let addrPacked: UInt64
    let body: String

    var id: UInt64 { addrPacked }
}

@MainActor @Observable
final class PairingAskModel {
    var asks: [PairingAsk] = []

    func drain() {
        let requests = DeskhubClient.ffiList(
            16, DHPairingRequest(),
            { dh_share_take_pairing_requests($0, $1) },
            { $0 }
        )
        guard !requests.isEmpty else { return }

        let paired = Set(DeskhubClient.ffiList(
            128, DHPairedDevice(),
            { dh_paired_devices($0, $1) },
            { DeskhubClient.cString($0.shortKey) }
        ))

        for request in requests {
            let shortKey = DeskhubClient.cString(request.shortKey)
            if paired.contains(shortKey) {
                dh_share_answer_pairing(request.addrPacked, true)
                continue
            }
            guard !asks.contains(where: { $0.addrPacked == request.addrPacked })
            else { continue }
            let address = DeskhubClient.buffered(64) {
                dh_format_address(request.addrPacked, $0, $1)
            }
            let name = DeskhubClient.cString(request.name)
            let body = DeskhubClient.buffered(512) {
                dh_pairing_request_body(name, address, shortKey, $0, $1)
            }
            asks.append(
                PairingAsk(addrPacked: request.addrPacked, body: body)
            )
        }
    }

    func answer(_ ask: PairingAsk, allow: Bool) {
        dh_share_answer_pairing(ask.addrPacked, allow)
        asks.removeAll { $0.addrPacked == ask.addrPacked }
    }

    func answerFirst(allow: Bool) {
        guard let ask = asks.first else { return }
        answer(ask, allow: allow)
    }

    func clear() {
        asks.removeAll()
    }
}

extension View {
    func pairingPrompt(_ model: PairingAskModel) -> some View {
        alert(
            DeskhubClient.string(DHStrPairingRequestTitle),
            isPresented: Binding(
                get: { !model.asks.isEmpty },
                set: { _ in }
            )
        ) {
            Button(DeskhubClient.string(DHStrPairingAllow)) { model.answerFirst(allow: true) }
            Button(DeskhubClient.string(DHStrPairingDeny), role: .cancel) {
                model.answerFirst(allow: false)
            }
        } message: {
            Text(model.asks.first?.body ?? "")
        }
    }
}
