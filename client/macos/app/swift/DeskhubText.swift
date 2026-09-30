import SwiftUI

func deskhubHeading(_ text: String) -> some View {
    Text(text)
        .font(.system(size: 19, weight: .bold))
        .foregroundStyle(DeskhubPalette.heading)
}

private let deskhubHintMaxWidth: CGFloat = 520

func deskhubSection(_ text: String) -> some View {
    Text(text)
        .font(.system(size: 15, weight: .bold))
        .foregroundStyle(DeskhubPalette.heading)
}

func deskhubHint(_ text: String) -> some View {
    Text(text)
        .foregroundStyle(DeskhubPalette.muted)
        .fixedSize(horizontal: false, vertical: true)
        .frame(maxWidth: deskhubHintMaxWidth, alignment: .leading)
}

let deskhubPrimaryButtonHeight: CGFloat = 26

extension View {
    func deskhubPrimaryLabel() -> some View {
        font(.system(size: 15, weight: .semibold))
            .frame(maxWidth: .infinity, minHeight: deskhubPrimaryButtonHeight)
    }
}
