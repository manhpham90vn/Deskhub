import SwiftUI

struct PairingQrShowButton: View {
    static let windowId = "pairingQr"

    let sharing: SharingModel
    @Environment(\.openWindow) private var openWindow
    @State private var unavailable = false

    var body: some View {
        Button(DeskhubClient.string(DHStrShowQrAction), action: show)
            .buttonStyle(.bordered)
            .alert("Deskhub", isPresented: $unavailable) {
                Button("OK", role: .cancel) {}
            } message: {
                Text(DeskhubClient.string(DHStrQrUnavailable))
            }
    }

    private func show() {
        if !sharing.qr.open {
            sharing.qr.show(port: sharing.sharingPort, bindIp: sharing.bindIp)
            guard sharing.qr.shown else {
                unavailable = true
                return
            }
        }
        openWindow(id: PairingQrShowButton.windowId)
    }
}

struct PairingQrWindow: View {
    private static let qrSide: CGFloat = 240
    private static let contentWidth: CGFloat = 300

    let sharing: SharingModel
    @Environment(\.dismissWindow) private var dismissWindow
    @State private var now = Date()

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            QrCodeView(modules: sharing.qr.modules)
                .frame(width: PairingQrWindow.qrSide, height: PairingQrWindow.qrSide)
                .frame(maxWidth: .infinity)
                .accessibilityLabel(sharing.qr.invite)
            HStack(spacing: 10) {
                Text(sharing.qr.invite)
                    .font(.system(.body, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .lineLimit(1)
                    .truncationMode(.middle)
                    .textSelection(.enabled)
                    .help(sharing.qr.invite)
                Spacer(minLength: 0)
                CopyButton { sharing.qr.invite }
                    .buttonStyle(.bordered)
                    .disabled(!sharing.qr.shown)
            }
            if sharing.qr.expired {
                Text(DeskhubClient.string(DHStrQrExpiredNote))
                    .foregroundStyle(DeskhubPalette.offline)
                    .fixedSize(horizontal: false, vertical: true)
                Button(DeskhubClient.string(DHStrNewQrAction)) {
                    sharing.qr.renew(port: sharing.sharingPort, bindIp: sharing.bindIp)
                }
                .buttonStyle(.borderedProminent)
                .tint(DeskhubPalette.accent)
                .frame(maxWidth: .infinity)
            } else {
                Text(DeskhubClient.qrExpiryLine(sharing.qr.secondsLeft(at: now)))
                    .foregroundStyle(DeskhubPalette.muted)
            }
            deskhubHint(DeskhubClient.string(DHStrQrHint))
        }
        .padding(16)
        .frame(width: PairingQrWindow.contentWidth, alignment: .topLeading)
        .task {
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(1))
                now = Date()
                if sharing.qr.shown, sharing.qr.secondsLeft(at: now) <= 0 {
                    sharing.qr.expire()
                }
            }
        }
        .onChange(of: sharing.isSharing) { _, live in
            if !live { dismissWindow(id: PairingQrShowButton.windowId) }
        }
        .onDisappear { sharing.qr.hide() }
    }
}
