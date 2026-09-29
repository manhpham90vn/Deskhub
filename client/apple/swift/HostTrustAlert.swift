import SwiftUI

private struct HostTrustAlert: ViewModifier {
    let connect: ConnectModel
    let onTrusted: @MainActor (String) -> Void

    private var showing: Binding<Bool> {
        Binding(
            get: { connect.pendingTrust != nil },
            set: { _ in }
        )
    }

    func body(content: Content) -> some View {
        content.alert(
            DeskhubClient.string(DHStrTrustNewHostTitle),
            isPresented: showing,
            presenting: connect.pendingTrust
        ) { _ in
            Button(DeskhubClient.string(DHStrCancelAction), role: .cancel) {
                connect.declinePendingHost()
            }
            Button(DeskhubClient.string(DHStrTrustNewHostAction)) {
                guard let address = connect.trustPendingHost() else { return }
                onTrusted(address)
            }
        } message: { pending in
            Text(pending.prompt)
        }
    }
}

extension View {
    func hostTrustAlert(
        _ connect: ConnectModel, onTrusted: @MainActor @escaping (String) -> Void
    ) -> some View {
        modifier(HostTrustAlert(connect: connect, onTrusted: onTrusted))
    }
}
