import Foundation
import UserNotifications

enum SystemNotifications {
    private static let consentOptions: UNAuthorizationOptions = [.alert, .sound]

    static func requestConsent() {
        UNUserNotificationCenter.current().requestAuthorization(options: consentOptions) { _, _ in }
    }

    static func post(identifier: String, title: String, body: String) {
        UNUserNotificationCenter.current().requestAuthorization(options: consentOptions) { granted, _ in
            guard granted else { return }
            deliver(identifier: identifier, title: title, body: body)
        }
    }

    private static func deliver(identifier: String, title: String, body: String) {
        let content = UNMutableNotificationContent()
        content.title = title
        content.body = body
        content.sound = .default
        let request = UNNotificationRequest(identifier: identifier, content: content, trigger: nil)
        UNUserNotificationCenter.current().add(request)
    }
}
