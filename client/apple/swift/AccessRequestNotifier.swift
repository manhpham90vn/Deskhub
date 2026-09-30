import Foundation

@MainActor
final class AccessRequestNotifier {
    static let shared = AccessRequestNotifier()

    private var announced = Set<String>()

    private init() {}

    func announce(_ requests: [AccessRequestRow]) {
        announced.formIntersection(requests.map(\.fingerprint))
        for request in requests where !announced.contains(request.fingerprint) {
            announced.insert(request.fingerprint)
            post(request)
        }
    }

    private func post(_ request: AccessRequestRow) {
        SystemNotifications.post(
            identifier: request.fingerprint,
            title: DeskhubClient.string(DHStrAccessRequestNotificationTitle),
            body: DeskhubClient.accessRequestNotification(
                name: request.name, address: request.address
            )
        )
    }
}
