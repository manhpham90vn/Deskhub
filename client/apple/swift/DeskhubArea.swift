import SwiftUI

struct DeskhubArea<Content: View>: View {
    private static var accentBarWidth: CGFloat { CGFloat(dh_settings_area_bar_width()) }

    let title: String
    @ViewBuilder let content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.system(size: 17, weight: .bold))
                .foregroundStyle(DeskhubPalette.heading)
            content
        }
        .padding(16)
        .padding(.leading, DeskhubArea.accentBarWidth)
        .frame(maxWidth: .infinity, alignment: .leading)
        .overlay(alignment: .leading) {
            Rectangle().fill(DeskhubPalette.accent).frame(width: DeskhubArea.accentBarWidth)
        }
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .overlay(
            RoundedRectangle(cornerRadius: 8).stroke(DeskhubPalette.rowLine, lineWidth: 1)
        )
    }
}
