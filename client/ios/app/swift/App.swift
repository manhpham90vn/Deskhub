import SwiftUI

@main
struct DeskhubApp: App {
    init() {
        if let container = BroadcastStatus.containerURL?.path {
            dh_use_shared_container(container)
        }
        NotificationBanners.shared.install()
        if !StartPage.isScreenshotRun {
            FilesHost.askNotificationConsent()
        }
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}
