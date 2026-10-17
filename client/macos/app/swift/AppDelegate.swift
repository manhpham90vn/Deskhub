import AppKit
import UserNotifications

extension NSApplication {
    var deskhubMainWindow: NSWindow? {
        windows.first { $0.identifier?.rawValue.hasPrefix("main") == true }
    }
}

private enum QuitReason {
    private static let attribute: AEKeyword = 0x7768_793F
    private static let sessionEnds: Set<OSType> = [
        0x6C6F_676F, 0x726C_676F, 0x7368_7574, 0x7265_7374, 0x7272_7374, 0x7273_646E,
    ]

    static var endsTheSession: Bool {
        guard let event = NSAppleEventManager.shared().currentAppleEvent,
              let reason = event.attributeDescriptor(forKeyword: attribute)
        else { return false }
        return sessionEnds.contains(reason.enumCodeValue)
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate, UNUserNotificationCenterDelegate {
    private static let restorationSettle = Duration.milliseconds(500)
    private static let sessionEndPoll = Duration.seconds(1)
    private static let sessionEndPatience = Duration.seconds(120)
    private static let finderBundleId = "com.apple.finder"
    private(set) var isTerminating = false

    func applicationDidFinishLaunching(_: Notification) {
        UNUserNotificationCenter.current().delegate = self
        Task { @MainActor in await AppDelegate.presentMainWindowUnlessRestored() }
    }

    @MainActor
    private static func presentMainWindowUnlessRestored() async {
        try? await Task.sleep(for: restorationSettle)
        guard NSApp.deskhubMainWindow?.isVisible != true else { return }
        _ = try? await NSWorkspace.shared.openApplication(
            at: Bundle.main.bundleURL, configuration: NSWorkspace.OpenConfiguration()
        )
    }

    func applicationShouldTerminateAfterLastWindowClosed(_: NSApplication) -> Bool {
        false
    }

    func applicationShouldTerminate(_: NSApplication) -> NSApplication.TerminateReply {
        guard QuitReason.endsTheSession, DeskhubShare.isRunning else {
            isTerminating = true
            return .terminateNow
        }
        Task { @MainActor [weak self] in await self?.keepSharingUntilOtherAppsQuit() }
        return .terminateLater
    }

    @MainActor
    private func keepSharingUntilOtherAppsQuit() async {
        let deadline = ContinuousClock.now + AppDelegate.sessionEndPatience
        while ContinuousClock.now < deadline {
            if !AppDelegate.otherAppsStillRunning {
                isTerminating = true
                NSApp.reply(toApplicationShouldTerminate: true)
                return
            }
            try? await Task.sleep(for: AppDelegate.sessionEndPoll)
        }
        NSApp.reply(toApplicationShouldTerminate: false)
    }

    private static var otherAppsStillRunning: Bool {
        let own = Bundle.main.bundleIdentifier
        return NSWorkspace.shared.runningApplications.contains { app in
            app.activationPolicy == .regular && !app.isTerminated
                && app.bundleIdentifier != own && app.bundleIdentifier != finderBundleId
        }
    }

    nonisolated func userNotificationCenter(
        _: UNUserNotificationCenter,
        willPresent _: UNNotification,
        withCompletionHandler completionHandler:
        @escaping (UNNotificationPresentationOptions) -> Void
    ) {
        completionHandler([.banner, .list, .sound])
    }

    nonisolated func userNotificationCenter(
        _: UNUserNotificationCenter,
        didReceive _: UNNotificationResponse,
        withCompletionHandler completionHandler: @escaping () -> Void
    ) {
        completionHandler()
        Task { @MainActor in AppDelegate.bringMainWindowForward() }
    }

    @MainActor
    private static func bringMainWindowForward() {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
        NSApp.deskhubMainWindow?.makeKeyAndOrderFront(nil)
    }
}
