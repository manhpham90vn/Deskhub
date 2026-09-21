import SwiftUI

#if os(macOS)
    import AppKit
#else
    import UIKit
#endif

struct PasscodeCard: View {
    let passcode: String

    @State private var copied = false

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(DeskhubClient.string(DHStrPasscodeShareHeading))
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(DeskhubPalette.muted)

            HStack(spacing: 14) {
                Text(DeskhubClient.passcodeDisplay(passcode))
                    .font(.system(size: 34, weight: .bold, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .textSelection(.enabled)
                Spacer(minLength: 0)
                if !passcode.isEmpty {
                    Button(copyLabel) { copy() }
                        .buttonStyle(.bordered)
                }
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(
            RoundedRectangle(cornerRadius: 10).fill(DeskhubPalette.accent.opacity(0.12))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 10)
                .stroke(DeskhubPalette.accent.opacity(0.35), lineWidth: 1)
        )
        .task(id: copied) {
            guard copied else { return }
            try? await Task.sleep(for: .seconds(1.5))
            copied = false
        }
    }

    private var copyLabel: String {
        DeskhubClient.string(copied ? DHStrPasscodeCopied : DHStrCopyPasscodeAction)
    }

    private func copy() {
        #if os(macOS)
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(passcode, forType: .string)
        #else
            UIPasteboard.general.string = passcode
        #endif
        copied = true
    }
}
