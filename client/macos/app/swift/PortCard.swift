import SwiftUI

struct PortCard: View {
    let port: Int

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
                CopyButton { String(port) }
                    .buttonStyle(.bordered)
            }
        }
        .padding(14)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 10).fill(DeskhubPalette.infoCard))
    }
}
