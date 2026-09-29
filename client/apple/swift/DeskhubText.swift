import SwiftUI

func deskhubHeading(_ text: String) -> some View {
    Text(text)
        .font(.system(size: 19, weight: .bold))
        .foregroundStyle(DeskhubPalette.heading)
}

func deskhubSection(_ text: String) -> some View {
    Text(text)
        .font(.system(size: 15, weight: .bold))
        .foregroundStyle(DeskhubPalette.heading)
        .padding(.top, 8)
}

func deskhubHint(_ text: String) -> some View {
    Text(text).foregroundStyle(DeskhubPalette.muted)
}

let deskhubPrimaryButtonHeight: CGFloat = 26

extension View {
    func deskhubPrimaryLabel() -> some View {
        font(.system(size: 15, weight: .semibold))
            .frame(maxWidth: .infinity, minHeight: deskhubPrimaryButtonHeight)
    }
}
