import SwiftUI

@main
struct DeskhubApp: App {
    init() {
        if let container = BroadcastStatus.containerURL?.path {
            dh_use_shared_container(container)
            DeskhubApp.excludeConfigFromBackup()
        }
        NotificationBanners.shared.install()
        if !StartPage.isScreenshotRun {
            SystemNotifications.requestConsent()
        }
    }

    private static let configDirCapacity = 1024

    private static func excludeConfigFromBackup() {
        let path = DeskhubClient.buffered(configDirCapacity) { dh_config_dir($0, $1) }
        guard !path.isEmpty else { return }
        var url = URL(fileURLWithPath: path, isDirectory: true)
        var values = URLResourceValues()
        values.isExcludedFromBackup = true
        try? url.setResourceValues(values)
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
