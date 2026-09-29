import SwiftUI

#if os(macOS)
    import AppKit
#else
    import UIKit
#endif

enum DeskhubPasteboard {
    @MainActor
    static func copy(_ text: String) {
        #if os(macOS)
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(text, forType: .string)
        #else
            UIPasteboard.general.string = text
        #endif
    }
}

struct CopyButton: View {
    private static let feedbackDuration = Duration.milliseconds(1500)

    let title: String
    let text: @MainActor () -> String

    @State private var copied = false

    init(_ title: String = DeskhubClient.string(DHStrCopyButton), text: @escaping @MainActor () -> String) {
        self.title = title
        self.text = text
    }

    var body: some View {
        Button(copied ? DeskhubClient.string(DHStrCopiedButton) : title) {
            let value = text()
            guard !value.isEmpty else { return }
            DeskhubPasteboard.copy(value)
            copied = true
        }
        .task(id: copied) {
            guard copied else { return }
            try? await Task.sleep(for: CopyButton.feedbackDuration)
            copied = false
        }
    }
}
