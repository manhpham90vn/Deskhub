import SwiftUI

@main
struct DeskhubApp: App {
    init() {
        if let container = BroadcastStatus.containerURL?.path {
            dh_set_data_dir(container)
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
