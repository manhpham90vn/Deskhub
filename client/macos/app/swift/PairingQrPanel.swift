import SwiftUI

struct PairingQrToggleButton: View {
    let model: PairingQrModel
    let port: UInt16
    let bindIp: String

    var body: some View {
        Button(DeskhubClient.string(model.shown ? DHStrHideQrAction : DHStrShowQrAction)) {
            model.toggle(port: port, bindIp: bindIp)
        }
        .buttonStyle(.bordered)
    }
}

struct PairingQrPanel: View {
    private static let qrSide: CGFloat = 240

    let model: PairingQrModel

    var body: some View {
        panel
    }

    private var panel: some View {
        VStack(alignment: .leading, spacing: 10) {
            QrCodeView(modules: model.modules)
                .frame(width: PairingQrPanel.qrSide, height: PairingQrPanel.qrSide)
                .frame(maxWidth: .infinity)
                .accessibilityLabel(model.invite)
            HStack(spacing: 10) {
                Text(model.invite)
                    .font(.system(.body, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .textSelection(.enabled)
                    .help(model.invite)
                Spacer(minLength: 0)
                CopyButton { model.invite }
                    .buttonStyle(.bordered)
            }
            deskhubHint(DeskhubClient.string(DHStrQrHint))
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(DeskhubPalette.infoCard)
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}
