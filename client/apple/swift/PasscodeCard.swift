import SwiftUI

#if os(macOS)
    import AppKit
#else
    import UIKit
#endif

struct PasscodeCard: View {
    let passcode: String
    var port: Int?

    @State private var copied = false
    @State private var portCopied = false

    var body: some View {
        HStack(spacing: 10) {
            card(passcodeContent)
            if let port {
                card(portContent(port))
            }
        }
        .task(id: copied) {
            guard copied else { return }
            try? await Task.sleep(for: .seconds(1.5))
            copied = false
        }
        .task(id: portCopied) {
            guard portCopied else { return }
            try? await Task.sleep(for: .seconds(1.5))
            portCopied = false
        }
    }

    private var passcodeContent: some View {
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
                    Button(copyLabel) {
                        copy(passcode)
                        copied = true
                    }
                    .buttonStyle(.bordered)
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private func portContent(_ port: Int) -> some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(DeskhubClient.string(DHStrUdpPortLabel))
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(DeskhubPalette.muted)

            HStack(spacing: 14) {
                Text(String(port))
                    .font(.system(size: 34, weight: .bold, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .textSelection(.enabled)
                Spacer(minLength: 0)
                Button(portCopied ? "Copied" : DeskhubClient.string(DHStrCopyButton)) {
                    copy(String(port))
                    portCopied = true
                }
                .buttonStyle(.bordered)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }

    private func card(_ content: some View) -> some View {
        content
            .padding(14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: 10).fill(DeskhubPalette.passcodeCard)
            )
    }

    private var copyLabel: String {
        DeskhubClient.string(copied ? DHStrPasscodeCopied : DHStrCopyPasscodeAction)
    }

    private func copy(_ value: String) {
        #if os(macOS)
            NSPasteboard.general.clearContents()
            NSPasteboard.general.setString(value, forType: .string)
        #else
            UIPasteboard.general.string = value
        #endif
    }
}
