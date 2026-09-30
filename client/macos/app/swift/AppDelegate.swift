import AppKit
import UserNotifications

final class AppDelegate: NSObject, NSApplicationDelegate, UNUserNotificationCenterDelegate {
    private(set) var isTerminating = false

    func applicationDidFinishLaunching(_: Notification) {
        UNUserNotificationCenter.current().delegate = self
    }

    func applicationShouldTerminateAfterLastWindowClosed(_: NSApplication) -> Bool {
        false
    }

    func applicationShouldTerminate(_: NSApplication) -> NSApplication.TerminateReply {
        isTerminating = true
        return .terminateNow
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
        let mainWindow = NSApp.windows.first { $0.identifier?.rawValue.hasPrefix("main") == true }
        mainWindow?.makeKeyAndOrderFront(nil)
    }
}
