import SwiftUI

struct IosQrInvitePanel: View {
    private static let widthFraction: CGFloat = 0.7

    let model: PairingQrModel

    var body: some View {
        QrCodeView(modules: model.modules)
            .containerRelativeFrame(.horizontal) { length, _ in
                length * IosQrInvitePanel.widthFraction
            }
            .frame(maxWidth: .infinity)
            .accessibilityLabel(model.invite)
        HStack(alignment: .center, spacing: 8) {
            Text(model.invite)
                .font(.system(size: iosHintSize, design: .monospaced))
                .foregroundStyle(DeskhubPalette.heading)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
            Button(DeskhubClient.string(DHStrCopyButton)) {
                DeskhubPasteboard.copy(model.invite)
            }
            .buttonStyle(.iosText())
        }
        iosHint(DeskhubClient.string(DHStrQrHint))
    }
}
