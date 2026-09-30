import SwiftUI

struct AccessRequestsSection: View {
    let model: AccessRequestsModel

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubSection(DeskhubClient.string(DHStrAccessRequestsHeading))
            if model.requests.isEmpty {
                deskhubHint(DeskhubClient.string(DHStrAccessRequestsEmpty))
            }
            requestRows
        }
    }

    private var requestRows: some View {
        Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 4) {
            ForEach(model.requests) { row in
                GridRow {
                    deskhubListCell(row.name.isEmpty ? DeskhubClient.string(DHStrUnnamedClient) : row.name, width: 180)
                    deskhubListCell(row.shortKey, width: 130, mono: true)
                        .help(row.fingerprint)
                        .accessibilityValue(row.fingerprint)
                    deskhubListCell(row.address, width: 170)
                    DeskhubRowActionButton(
                        title: DeskhubClient.string(DHStrApproveAction),
                        tint: DeskhubPalette.accent
                    ) { model.approve(row) }
                    DeskhubRowActionButton(
                        title: DeskhubClient.string(DHStrDenyAction),
                        tint: DeskhubPalette.offline
                    ) { model.deny(row) }
                }
            }
        }
    }
}
