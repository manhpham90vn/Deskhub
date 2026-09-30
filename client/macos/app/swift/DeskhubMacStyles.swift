import SwiftUI

extension DeskhubPalette {
    static let accentPressed = macTheme(DHThemeAccentPressed)
    static let accentDisabled = macTheme(DHThemeAccentDisabled)
    static let page = macTheme(DHThemePage)
    static let onlineText = adaptiveColor(
        light: PaletteRgb(packed: 0x075E2B),
        dark: PaletteRgb(packed: dh_theme_color(DHThemeOnline, true))
    )

    private static func macTheme(_ color: DHThemeColor) -> Color {
        adaptiveColor(
            light: PaletteRgb(packed: dh_theme_color(color, false)),
            dark: PaletteRgb(packed: dh_theme_color(color, true))
        )
    }
}

enum DeskhubPrimaryMetrics {
    static let buttonHeight: CGFloat = 46
    static let pickerHeight: CGFloat = 170
}

struct DeskhubPrimaryButtonStyle: ButtonStyle {
    let fill: Color
    var disabledFill: Color = DeskhubPalette.accentDisabled

    @Environment(\.isEnabled) private var isEnabled

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: 15, weight: .bold))
            .foregroundStyle(Color.white)
            .frame(maxWidth: .infinity)
            .frame(height: DeskhubPrimaryMetrics.buttonHeight)
            .background(RoundedRectangle(cornerRadius: 6).fill(isEnabled ? fill : disabledFill))
            .opacity(configuration.isPressed ? 0.85 : 1)
            .contentShape(RoundedRectangle(cornerRadius: 6))
    }
}

private struct DeskhubBorderedBox: ViewModifier {
    let minHeight: CGFloat

    func body(content: Content) -> some View {
        content
            .frame(maxWidth: .infinity, minHeight: minHeight, alignment: .topLeading)
            .background(DeskhubPalette.page)
            .overlay(Rectangle().stroke(DeskhubPalette.rowLine, lineWidth: 1))
    }
}

extension View {
    func deskhubBorderedBox(minHeight: CGFloat) -> some View {
        modifier(DeskhubBorderedBox(minHeight: minHeight))
    }
}
