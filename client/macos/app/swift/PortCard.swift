import AppKit
import SwiftUI

struct PortCard: View {
    let port: Int

    @State private var copied = false

    var body: some View {
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
                Button(copied ? "Copied" : DeskhubClient.string(DHStrCopyButton)) {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(String(port), forType: .string)
                    copied = true
                }
                .buttonStyle(.bordered)
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 10).fill(DeskhubPalette.infoCard))
        .task(id: copied) {
            guard copied else { return }
            try? await Task.sleep(for: .seconds(1.5))
            copied = false
        }
    }
}
