import SwiftUI

struct SavedHostsSection: View {
    let trustedHostsRevision: Int
    let onConnect: @MainActor (String) -> Void
    @State private var model = SavedHostsModel()

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubSection(DeskhubClient.string(DHStrSavedHostsHeading))
            deskhubHint(DeskhubClient.string(DHStrSavedHostsHint))
            hostList
        }
        .onAppear(perform: model.refresh)
        .onChange(of: trustedHostsRevision) { _, _ in model.refresh() }
    }

    @ViewBuilder
    private var hostList: some View {
        if !model.listError.isEmpty {
            Text(model.listError).foregroundStyle(DeskhubPalette.offline)
        } else if model.hosts.isEmpty {
            deskhubHint(DeskhubClient.string(DHStrSavedHostsEmpty))
        }
        ForEach(model.hosts) { host in
            hostRow(host)
        }
    }

    private func hostRow(_ host: SavedHostRow) -> some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(host.alias).foregroundStyle(DeskhubPalette.heading)
                Text(host.endpoint)
                    .font(.caption)
                    .foregroundStyle(DeskhubPalette.muted)
                    .textSelection(.enabled)
                Text(host.shortKey)
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.muted)
                    .help(host.fingerprint)
                    .accessibilityValue(host.fingerprint)
            }
            Spacer(minLength: 0)
            Button(DeskhubClient.string(DHStrConnectButton)) {
                onConnect(host.endpoint)
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.small)
            .tint(DeskhubPalette.accent)
            Button(DeskhubClient.string(DHStrRemoveHostAction)) {
                model.remove(host)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .tint(DeskhubPalette.offline)
        }
        .padding(.vertical, 2)
    }
}
