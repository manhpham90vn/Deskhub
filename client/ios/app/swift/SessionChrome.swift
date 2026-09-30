import SwiftUI

private let closeButtonSide: CGFloat = 44
private let expandButtonSide: CGFloat = 48
private let dimmedAlpha = 0.35
private let sessionButtonFill = Color.black.opacity(0.45)
private let sessionButtonStroke = Color.white.opacity(0.25)

let sessionBannerFill = Color.black.opacity(0.75)
let sessionOverlayDim = Color.black.opacity(0.7)

struct SessionCloseButton: View {
    let action: () -> Void
    var enabled = true

    var body: some View {
        let tint = enabled ? 1.0 : dimmedAlpha
        Button(action: action) {
            Text(verbatim: "\u{2715}")
                .font(.system(size: 17))
                .foregroundStyle(.white.opacity(tint))
                .frame(width: closeButtonSide, height: closeButtonSide)
                .background(sessionButtonFill, in: Circle())
                .overlay(Circle().strokeBorder(sessionButtonStroke.opacity(tint), lineWidth: 1))
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .accessibilityLabel("Close")
    }
}

struct SessionExpandButton: View {
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Text(verbatim: "\u{2630}")
                .font(.system(size: 17))
                .foregroundStyle(.white)
                .frame(width: expandButtonSide, height: expandButtonSide)
                .background(sessionButtonFill, in: Circle())
                .overlay(Circle().strokeBorder(sessionButtonStroke, lineWidth: 1))
        }
        .buttonStyle(.plain)
        .accessibilityLabel("Show controls")
    }
}

struct SessionBanner: View {
    let text: String

    var body: some View {
        Text(text)
            .foregroundStyle(.white)
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .background(sessionBannerFill)
            .padding(12)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
            .allowsHitTesting(false)
    }
}

struct DeskhubOutlinedButtonStyle: ButtonStyle {
    var tint: Color = DeskhubPalette.accent
    var horizontalPadding: CGFloat = 24
    var verticalPadding: CGFloat = 10

    func makeBody(configuration: Configuration) -> some View {
        DeskhubOutlinedButtonBody(
            configuration: configuration,
            tint: tint,
            horizontalPadding: horizontalPadding,
            verticalPadding: verticalPadding
        )
    }
}

private struct DeskhubOutlinedButtonBody: View {
    let configuration: ButtonStyleConfiguration
    let tint: Color
    let horizontalPadding: CGFloat
    let verticalPadding: CGFloat
    @Environment(\.isEnabled) private var isEnabled

    var body: some View {
        configuration.label
            .font(.system(size: 14, weight: .medium))
            .lineLimit(1)
            .foregroundStyle(tint.opacity(isEnabled ? 1 : 0.38))
            .padding(.horizontal, horizontalPadding)
            .padding(.vertical, verticalPadding)
            .background(Capsule().fill(Color.white.opacity(configuration.isPressed ? 0.12 : 0)))
            .overlay(
                Capsule().strokeBorder(Color.white.opacity(isEnabled ? 0.3 : 0.12), lineWidth: 1)
            )
            .contentShape(Capsule())
    }
}

extension ButtonStyle where Self == DeskhubOutlinedButtonStyle {
    static func deskhubOutlined(
        tint: Color = DeskhubPalette.accent,
        horizontalPadding: CGFloat = 24,
        verticalPadding: CGFloat = 10
    ) -> DeskhubOutlinedButtonStyle {
        DeskhubOutlinedButtonStyle(
            tint: tint, horizontalPadding: horizontalPadding, verticalPadding: verticalPadding
        )
    }
}

struct SessionTextButton: View {
    let title: String
    let action: () -> Void

    init(_ title: String, action: @escaping () -> Void) {
        self.title = title
        self.action = action
    }

    var body: some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 14, weight: .medium))
                .foregroundStyle(DeskhubPalette.accent)
                .padding(.horizontal, 12)
                .padding(.vertical, 10)
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
