import Foundation
import Observation
import UserNotifications

@MainActor @Observable
final class FilesHost {
    static let shared = FilesHost()

    private(set) var receiving = false
    @ObservationIgnored private var activeSettings = ""

    private init() {}

    func run() async {
        while !Task.isCancelled {
            sync()
            let delivered = await ReceivedFiles.sweep(
                keepPartFiles: receiving || BroadcastStatus.broadcastProcessAlive
            )
            if !delivered.isEmpty { Self.notifyArrived(delivered) }
            try? await Task.sleep(for: .seconds(1))
        }
    }

    static func askNotificationConsent() {
        let center = UNUserNotificationCenter.current()
        center.requestAuthorization(options: [.alert, .sound]) { _, _ in }
    }

    private static func notifyArrived(_ names: [String]) {
        let content = UNMutableNotificationContent()
        content.title = DeskhubClient.string(DHStrTransferArrivedTitle)
        content.body = names.joined(separator: ", ")
        let request = UNNotificationRequest(
            identifier: UUID().uuidString, content: content, trigger: nil
        )
        UNUserNotificationCenter.current().add(request)
    }

    func stop() {
        guard receiving else { return }
        dh_share_stop()
        receiving = false
    }

    private func sync() {
        let wanted = !BroadcastStatus.broadcastProcessAlive
        if wanted, receiving, activeSettings != FilesHost.settingsSignature() { stop() }
        if wanted, !receiving { start() }
        if !wanted, receiving { stop() }
        if receiving, !dh_share_running() { receiving = false }
    }

    private static func settingsSignature() -> String {
        String(dh_settings_load().port)
    }

    private func start() {
        guard let dir = ReceivedFiles.incomingDir else { return }
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        ReceivedFiles.removeStaleParts()
        dh_set_transfer_dir(dir.path)
        let stored = dh_settings_load()
        receiving = dh_share_start(nil, 0, 0, 0, 0, UInt16(stored.port), false, false, true)
        if receiving { activeSettings = FilesHost.settingsSignature() }
    }
}

@MainActor
final class NotificationBanners: NSObject, UNUserNotificationCenterDelegate {
    static let shared = NotificationBanners()

    func install() {
        UNUserNotificationCenter.current().delegate = self
    }

    nonisolated func userNotificationCenter(
        _: UNUserNotificationCenter,
        willPresent _: UNNotification,
        withCompletionHandler completionHandler:
        @escaping (UNNotificationPresentationOptions) -> Void
    ) {
        completionHandler([.banner, .list])
    }
}
