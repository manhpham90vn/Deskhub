import SwiftUI

extension DeskhubPalette {
    static let page = adaptiveColor(
        light: PaletteRgb(packed: dh_theme_color(DHThemePage, false)),
        dark: PaletteRgb(packed: dh_theme_color(DHThemePage, true))
    )
}

let iosHeadingSize: CGFloat = 22
let iosSectionSize: CGFloat = 16
let iosBodySize: CGFloat = 14
let iosHintSize: CGFloat = 12
let iosLabelSize: CGFloat = 11
let iosPagePadding: CGFloat = 16
let iosPageSpacing: CGFloat = 16

func iosHeading(_ text: String) -> some View {
    Text(text)
        .font(.system(size: iosHeadingSize, weight: .bold))
        .foregroundStyle(DeskhubPalette.heading)
}

func iosSection(_ text: String) -> some View {
    Text(text)
        .font(.system(size: iosSectionSize, weight: .bold))
        .foregroundStyle(DeskhubPalette.heading)
}

func iosNote(_ text: String) -> some View {
    Text(text)
        .font(.system(size: iosBodySize))
        .foregroundStyle(DeskhubPalette.muted)
}

func iosHint(_ text: String) -> some View {
    Text(text)
        .font(.system(size: iosHintSize))
        .foregroundStyle(DeskhubPalette.muted)
}

func iosError(_ text: String) -> some View {
    Text(text)
        .font(.system(size: iosBodySize))
        .foregroundStyle(DeskhubPalette.offline)
}

private let iosButtonMinHeight: CGFloat = 40
private let iosButtonHorizontalPadding: CGFloat = 24
private let iosTextButtonHorizontalPadding: CGFloat = 12
private let iosPressedOpacity = 0.7
private let iosDisabledOpacity = 0.38

private func iosButtonOpacity(enabled: Bool, pressed: Bool) -> Double {
    guard enabled else { return iosDisabledOpacity }
    return pressed ? iosPressedOpacity : 1
}

struct IosFilledButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled
    private let fullWidth: Bool

    init(fullWidth: Bool = false) {
        self.fullWidth = fullWidth
    }

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: iosBodySize, weight: .semibold))
            .foregroundStyle(.white)
            .padding(.horizontal, iosButtonHorizontalPadding)
            .frame(maxWidth: fullWidth ? .infinity : nil, minHeight: iosButtonMinHeight)
            .background(DeskhubPalette.accent, in: Capsule())
            .opacity(iosButtonOpacity(enabled: isEnabled, pressed: configuration.isPressed))
    }
}

struct IosOutlinedButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled
    private let fullWidth: Bool

    init(fullWidth: Bool = false) {
        self.fullWidth = fullWidth
    }

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: iosBodySize, weight: .semibold))
            .foregroundStyle(DeskhubPalette.accent)
            .padding(.horizontal, iosButtonHorizontalPadding)
            .frame(maxWidth: fullWidth ? .infinity : nil, minHeight: iosButtonMinHeight)
            .overlay(Capsule().strokeBorder(DeskhubPalette.accent, lineWidth: 1))
            .contentShape(Capsule())
            .opacity(iosButtonOpacity(enabled: isEnabled, pressed: configuration.isPressed))
    }
}

struct IosTextButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) private var isEnabled
    private let color: Color

    init(color: Color = DeskhubPalette.accent) {
        self.color = color
    }

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .font(.system(size: iosBodySize, weight: .semibold))
            .foregroundStyle(color)
            .padding(.horizontal, iosTextButtonHorizontalPadding)
            .frame(minHeight: iosButtonMinHeight)
            .contentShape(Rectangle())
            .opacity(iosButtonOpacity(enabled: isEnabled, pressed: configuration.isPressed))
    }
}

extension ButtonStyle where Self == IosFilledButtonStyle {
    static func iosFilled(fullWidth: Bool = false) -> IosFilledButtonStyle {
        IosFilledButtonStyle(fullWidth: fullWidth)
    }
}

extension ButtonStyle where Self == IosOutlinedButtonStyle {
    static func iosOutlined(fullWidth: Bool = false) -> IosOutlinedButtonStyle {
        IosOutlinedButtonStyle(fullWidth: fullWidth)
    }
}

extension ButtonStyle where Self == IosTextButtonStyle {
    static func iosText(color: Color = DeskhubPalette.accent) -> IosTextButtonStyle {
        IosTextButtonStyle(color: color)
    }
}

private let iosFieldCornerRadius: CGFloat = 4
private let iosFieldMinHeight: CGFloat = 48
private let iosFieldHorizontalPadding: CGFloat = 12

extension View {
    func iosFieldBorder() -> some View {
        padding(.horizontal, iosFieldHorizontalPadding)
            .frame(minHeight: iosFieldMinHeight)
            .overlay(
                RoundedRectangle(cornerRadius: iosFieldCornerRadius)
                    .strokeBorder(DeskhubPalette.muted, lineWidth: 1)
            )
    }

    func iosPageBackground() -> some View {
        background { DeskhubPalette.page.ignoresSafeArea() }
            .toolbarBackground(DeskhubPalette.page, for: .tabBar)
            .toolbarBackground(.visible, for: .tabBar)
    }
}

enum DigitsOnly {
    static let portLength = 5

    static func limit(_ text: String, to maxLength: Int) -> String {
        String(text.filter { $0.isASCII && $0.isNumber }.prefix(maxLength))
    }
}
