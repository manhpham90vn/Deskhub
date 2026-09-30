import SwiftUI

struct IosAccessRequestRow: View {
    let row: AccessRequestRow
    let model: AccessRequestsModel

    var body: some View {
        HStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 2) {
                Text(row.name.isEmpty ? DeskhubClient.string(DHStrUnnamedClient) : row.name)
                    .foregroundStyle(DeskhubPalette.heading)
                iosHint("\(row.shortKey)  ·  \(row.address)")
                    .accessibilityValue(row.fingerprint)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Button(DeskhubClient.string(DHStrApproveAction)) {
                model.approve(row)
            }
            .buttonStyle(.iosText())
            Button(DeskhubClient.string(DHStrDenyAction)) {
                model.deny(row)
            }
            .buttonStyle(.iosText(color: DeskhubPalette.offline))
        }
    }
}

struct IosAccessRequestsSection: View {
    let model: AccessRequestsModel

    var body: some View {
        iosSection(DeskhubClient.string(DHStrAccessRequestsHeading))
        if model.requests.isEmpty {
            iosNote(DeskhubClient.string(DHStrAccessRequestsEmpty))
        }
        ForEach(model.requests) { row in
            IosAccessRequestRow(row: row, model: model)
        }
    }
}
