import SwiftUI

struct SavedHostsSection: View {
    let trustedHostsRevision: Int
    let onConnect: @MainActor (String) -> Void
    @State private var model = SavedHostsModel()

    private static let itemSpacing: CGFloat = 6
    private static let nameColumnWidth: CGFloat = 150
    private static let addressColumnWidth: CGFloat = 170
    private static let keyColumnWidth: CGFloat = 130

    var body: some View {
        VStack(alignment: .leading, spacing: Self.itemSpacing) {
            deskhubSection(DeskhubClient.string(DHStrSavedHostsHeading))
            deskhubHint(DeskhubClient.string(DHStrSavedHostsHint))
            hostList
        }
        .onAppear(perform: model.refresh)
        .onChange(of: trustedHostsRevision) { _, _ in model.refresh() }
    }

    @ViewBuilder
    private var hostList: some View {
        DeskhubListFrame {
            HStack(spacing: DeskhubListMetrics.columnGap) {
                deskhubListHeaderCell(DeskhubClient.string(DHStrHostNameLabel), width: Self.nameColumnWidth)
                deskhubListHeaderCell(
                    DeskhubClient.string(DHStrHostLastAddressLabel),
                    width: Self.addressColumnWidth
                )
                deskhubListHeaderCell(DeskhubClient.string(DHStrHostKeyLabel), width: Self.keyColumnWidth)
            }
            ForEach(model.hosts) { host in
                hostRow(host)
            }
        }
        if !model.storeUnreadable, model.hosts.isEmpty {
            deskhubHint(DeskhubClient.string(DHStrSavedHostsEmpty))
        }
        if !model.listError.isEmpty {
            Text(model.listError)
                .foregroundStyle(DeskhubPalette.errorText)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private func hostRow(_ host: SavedHostRow) -> some View {
        HStack(spacing: DeskhubListMetrics.columnGap) {
            deskhubListCell(host.alias, width: Self.nameColumnWidth)
            deskhubListCell(host.endpoint, width: Self.addressColumnWidth)
            deskhubListCell(host.shortKey, width: Self.keyColumnWidth, mono: true)
                .help(host.fingerprint)
                .accessibilityValue(host.fingerprint)
            DeskhubRowActionButton(
                title: DeskhubClient.string(DHStrConnectButton),
                tint: DeskhubPalette.accent
            ) {
                onConnect(host.endpoint)
            }
            DeskhubRowActionButton(
                title: DeskhubClient.string(DHStrRemoveHostAction),
                tint: DeskhubPalette.offline
            ) {
                model.remove(host)
            }
        }
    }
}
