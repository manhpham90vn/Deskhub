import SwiftUI

enum DeskhubListMetrics {
    static let minHeight: CGFloat = 130
    static let padding: CGFloat = 12
    static let columnGap: CGFloat = 8
    static let rowGap: CGFloat = 4
    static let headerFontSize: CGFloat = 11
    static let actionWidth = CGFloat(dh_host_table_metrics().actionWidth)
    static let actionHeight: CGFloat = 26
    static let actionCornerRadius: CGFloat = 4
}

struct DeskhubListFrame<Content: View>: View {
    @ViewBuilder let content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: DeskhubListMetrics.rowGap) {
            content
        }
        .padding(DeskhubListMetrics.padding)
        .frame(maxWidth: .infinity, minHeight: DeskhubListMetrics.minHeight, alignment: .topLeading)
        .overlay(Rectangle().stroke(DeskhubPalette.rowLine, lineWidth: 1))
    }
}

func deskhubListHeaderCell(_ text: String, width: CGFloat) -> some View {
    Text(text)
        .font(.system(size: DeskhubListMetrics.headerFontSize, weight: .bold))
        .foregroundStyle(DeskhubPalette.muted)
        .lineLimit(1)
        .frame(width: width, alignment: .leading)
}

func deskhubListCell(_ text: String, width: CGFloat, mono: Bool = false) -> some View {
    Text(text)
        .font(mono ? Font.system(.body, design: .monospaced) : Font.body)
        .foregroundStyle(DeskhubPalette.heading)
        .lineLimit(1)
        .truncationMode(.tail)
        .frame(width: width, alignment: .leading)
}

struct DeskhubRowActionButton: View {
    let title: String
    let tint: Color
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(title)
                .foregroundStyle(Color.white)
                .lineLimit(1)
                .frame(width: DeskhubListMetrics.actionWidth, height: DeskhubListMetrics.actionHeight)
                .background(
                    RoundedRectangle(cornerRadius: DeskhubListMetrics.actionCornerRadius).fill(tint)
                )
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
