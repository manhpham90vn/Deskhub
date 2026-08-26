import Foundation

enum StartPage {
    private static let key = "DeskhubStartPage"

    static func index() -> Int? {
        let defaults = UserDefaults.standard
        guard defaults.object(forKey: key) != nil else { return nil }
        return defaults.integer(forKey: key)
    }

    static var isScreenshotRun: Bool {
        index() != nil
    }
}
