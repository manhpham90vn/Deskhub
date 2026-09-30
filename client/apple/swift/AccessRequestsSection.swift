import SwiftUI

struct AccessRequestsSection: View {
    let model: AccessRequestsModel

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubSection(DeskhubClient.string(DHStrAccessRequestsHeading))
            if model.requests.isEmpty {
                deskhubHint(DeskhubClient.string(DHStrAccessRequestsEmpty))
            }
            ForEach(model.requests) { row in
                requestRow(row)
            }
        }
    }

    private func requestRow(_ row: AccessRequestRow) -> some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(row.name.isEmpty ? "(unnamed)" : row.name)
                    .foregroundStyle(DeskhubPalette.heading)
                HStack(spacing: 6) {
                    Text(row.shortKey)
                        .font(.system(.caption, design: .monospaced))
                        .help(row.fingerprint)
                        .accessibilityValue(row.fingerprint)
                    Text("·")
                    Text(row.address)
                    Text("·")
                    Text(row.requestedAt)
                }
                .font(.caption)
                .foregroundStyle(DeskhubPalette.muted)
            }
            Spacer(minLength: 0)
            Button(DeskhubClient.string(DHStrApproveAction)) {
                model.approve(row)
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.small)
            .tint(DeskhubPalette.accent)
            Button(DeskhubClient.string(DHStrDenyAction)) {
                model.deny(row)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
            .tint(DeskhubPalette.offline)
        }
        .padding(.vertical, 2)
    }
}
