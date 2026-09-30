import SwiftUI

struct MainMenuSidebar: View {
    private static let width: CGFloat = 180
    private static let pad: CGFloat = 16
    private static let linkLeadingPad: CGFloat = 8
    private static let titleFontSize: CGFloat = 24

    @Binding var page: DeskhubPage

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Deskhub")
                .font(.system(size: MainMenuSidebar.titleFontSize, weight: .bold))
                .foregroundStyle(.white)
                .padding(MainMenuSidebar.pad)

            ForEach(DeskhubPage.allCases) { item in
                SidebarItem(item: item, selected: page == item) { page = item }
            }

            Spacer(minLength: 0)

            if let url = URL(string: DeskhubClient.string(DHStrProjectUrl)) {
                Link(DeskhubClient.string(DHStrProjectLinkLabel), destination: url)
                    .foregroundStyle(DeskhubPalette.navText)
                    .padding(.leading, MainMenuSidebar.linkLeadingPad)
                    .padding(.trailing, MainMenuSidebar.pad)
            }

            Text(DeskhubClient.buffered(64) { dh_version_line($0, $1) })
                .foregroundStyle(DeskhubPalette.footnote)
                .padding([.leading, .trailing, .bottom], MainMenuSidebar.pad)
        }
        .frame(width: MainMenuSidebar.width)
        .frame(maxHeight: .infinity)
        .background(DeskhubPalette.sidebar)
    }
}

private struct SidebarItem: View {
    private static let fontSize: CGFloat = 15
    private static let height: CGFloat = 42

    let item: DeskhubPage
    let selected: Bool
    let onClick: () -> Void

    @State private var hovering = false

    var body: some View {
        Button(action: onClick) {
            Text(item.label)
                .font(.system(size: SidebarItem.fontSize, weight: selected ? .bold : .regular))
                .foregroundStyle(selected ? Color.white : DeskhubPalette.navText)
                .frame(maxWidth: .infinity, minHeight: SidebarItem.height, alignment: .leading)
                .padding(.horizontal, 16)
                .background(
                    RoundedRectangle(cornerRadius: 8).fill(fill)
                )
                .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .onHover { hovering = $0 }
        .padding(.horizontal, 10)
        .padding(.bottom, 10)
    }

    private var fill: Color {
        if selected { return DeskhubPalette.accent }
        return hovering ? DeskhubPalette.sidebarHover : Color.clear
    }
}
