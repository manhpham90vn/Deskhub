import Foundation
import Observation

@MainActor @Observable
final class AccessRequestsModel {
    private(set) var requests: [AccessRequestRow] = []
    @ObservationIgnored private var generation: UInt64?

    func refresh() {
        let current = DeskhubClient.accessRequestsGeneration
        guard current != generation else { return }
        generation = current
        requests = DeskhubClient.accessRequests()
    }

    func approve(_ row: AccessRequestRow) {
        _ = DeskhubClient.approveAccess(row.fingerprint)
        reload()
    }

    func deny(_ row: AccessRequestRow) {
        _ = DeskhubClient.denyAccess(row.fingerprint)
        reload()
    }

    func clear() {
        generation = nil
        requests = []
    }

    private func reload() {
        generation = nil
        refresh()
    }
}
