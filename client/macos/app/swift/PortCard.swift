import SwiftUI

struct PortCard: View {
    let port: Int

    var body: some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                deskhubHint(DeskhubClient.string(DHStrUdpPortLabel))
                Text(String(port))
                    .font(.system(size: 28, weight: .bold, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .textSelection(.enabled)
            }
            Spacer(minLength: 0)
            CopyButton { String(port) }
                .buttonStyle(.bordered)
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 8).fill(DeskhubPalette.infoCard))
    }
}
