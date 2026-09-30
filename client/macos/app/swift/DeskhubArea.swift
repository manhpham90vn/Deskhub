import SwiftUI

struct DeskhubArea<Content: View>: View {
    private static var accentBarWidth: CGFloat { CGFloat(dh_settings_area_bar_width()) }

    let title: String
    @ViewBuilder let content: Content

    private static var borderWidth: CGFloat { 1 }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.system(size: 16, weight: .bold))
                .foregroundStyle(DeskhubPalette.heading)
            content
        }
        .padding(16)
        .padding(.leading, DeskhubArea.accentBarWidth)
        .frame(maxWidth: .infinity, alignment: .leading)
        .overlay(alignment: .leading) {
            Rectangle().fill(DeskhubPalette.accent).frame(width: DeskhubArea.accentBarWidth)
        }
        .overlay(alignment: .top) {
            Rectangle().fill(DeskhubPalette.rowLine).frame(height: DeskhubArea.borderWidth)
        }
        .overlay(alignment: .bottom) {
            Rectangle().fill(DeskhubPalette.rowLine).frame(height: DeskhubArea.borderWidth)
        }
        .overlay(alignment: .trailing) {
            Rectangle().fill(DeskhubPalette.rowLine).frame(width: DeskhubArea.borderWidth)
        }
    }
}
