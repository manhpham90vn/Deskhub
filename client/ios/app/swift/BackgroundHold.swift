import UIKit

@MainActor
final class BackgroundHold {
    static let shared = BackgroundHold()

    private var task = UIBackgroundTaskIdentifier.invalid

    private init() {}

    func begin() {
        guard task == .invalid else { return }
        task = UIApplication.shared.beginBackgroundTask(withName: "Deskhub session") {
            MainActor.assumeIsolated { BackgroundHold.shared.end() }
        }
    }

    func end() {
        guard task != .invalid else { return }
        UIApplication.shared.endBackgroundTask(task)
        task = .invalid
    }
}
