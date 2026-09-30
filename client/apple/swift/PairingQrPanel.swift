import SwiftUI

struct QrCodeView: View {
    private static let quietZoneModules = 4

    let modules: [[Bool]]

    var body: some View {
        Canvas { context, size in
            context.fill(Path(CGRect(origin: .zero, size: size)), with: .color(.white))
            guard !modules.isEmpty else { return }
            let cells = CGFloat(modules.count + 2 * QrCodeView.quietZoneModules)
            let cell = min(size.width, size.height) / cells
            let originX = (size.width - cell * cells) / 2 + cell * CGFloat(QrCodeView.quietZoneModules)
            let originY = (size.height - cell * cells) / 2 + cell * CGFloat(QrCodeView.quietZoneModules)
            var dark = Path()
            for (rowIndex, row) in modules.enumerated() {
                for (columnIndex, isDark) in row.enumerated() where isDark {
                    dark.addRect(CGRect(
                        x: originX + CGFloat(columnIndex) * cell,
                        y: originY + CGFloat(rowIndex) * cell,
                        width: cell, height: cell
                    ))
                }
            }
            context.fill(dark, with: .color(.black))
        }
        .aspectRatio(1, contentMode: .fit)
    }
}

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
        VStack(alignment: .leading, spacing: 10) {
            QrCodeView(modules: model.modules)
                .frame(width: PairingQrPanel.qrSide, height: PairingQrPanel.qrSide)
                .accessibilityLabel(model.invite)
            HStack(alignment: .top, spacing: 12) {
                Text(model.invite)
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.muted)
                    .textSelection(.enabled)
                    .lineLimit(3)
                    .truncationMode(.middle)
                Spacer(minLength: 0)
                CopyButton { model.invite }
                    .buttonStyle(.bordered)
                    .controlSize(.small)
            }
            deskhubHint(DeskhubClient.string(DHStrQrHint))
        }
        .padding(12)
        .background(DeskhubPalette.panelIdle)
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}
