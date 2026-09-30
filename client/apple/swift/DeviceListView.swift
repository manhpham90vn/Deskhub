import SwiftUI

struct PaletteRgb {
    let red: Double
    let green: Double
    let blue: Double

    init(packed: UInt32) {
        red = Double((packed >> 16) & 0xFF) / 255
        green = Double((packed >> 8) & 0xFF) / 255
        blue = Double(packed & 0xFF) / 255
    }
}

#if os(macOS)
    import AppKit

    func adaptiveColor(light: PaletteRgb, dark: PaletteRgb) -> Color {
        Color(nsColor: NSColor(name: nil) { appearance in
            let rgb = appearance.bestMatch(from: [.aqua, .darkAqua]) == .darkAqua ? dark : light
            return NSColor(srgbRed: rgb.red, green: rgb.green, blue: rgb.blue, alpha: 1)
        })
    }
#else
    import UIKit

    func adaptiveColor(light: PaletteRgb, dark: PaletteRgb) -> Color {
        Color(uiColor: UIColor { traits in
            let rgb = traits.userInterfaceStyle == .dark ? dark : light
            return UIColor(red: rgb.red, green: rgb.green, blue: rgb.blue, alpha: 1)
        })
    }
#endif

enum DeskhubPalette {
    static let sidebar = theme(DHThemeSidebar)
    static let sidebarHover = theme(DHThemeSidebarHover)
    static let accent = theme(DHThemeAccent)
    static let navText = theme(DHThemeNavText)
    static let footnote = theme(DHThemeFootnote)
    static let heading = theme(DHThemeHeading)
    static let muted = theme(DHThemeMuted)
    static let online = theme(DHThemeOnline)
    static let offline = theme(DHThemeOffline)
    static let errorText = theme(DHThemeOfflinePressed)
    static let warning = theme(DHThemeWarning)
    static let rowLine = theme(DHThemeRowLine)
    static let viewerRow = theme(DHThemeViewerRow)
    static let panelIdle = theme(DHThemePanelIdle)
    static let panelBusy = theme(DHThemePanelBusy)
    static let panelLive = theme(DHThemePanelLive)
    static let infoCard = theme(DHThemeInfoCard)

    private static func theme(_ color: DHThemeColor) -> Color {
        adaptiveColor(
            light: PaletteRgb(packed: dh_theme_color(color, false)),
            dark: PaletteRgb(packed: dh_theme_color(color, true))
        )
    }
}

struct DeviceListView: View {
    let rows: [DeviceListRow]
    let enabled: Bool
    let onPick: (DeviceListRow) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            ForEach(rows) { row in
                Button {
                    onPick(row)
                } label: {
                    HStack(spacing: 12) {
                        VStack(alignment: .leading, spacing: 1) {
                            Text(row.title).foregroundStyle(DeskhubPalette.heading)
                            if !row.detail.isEmpty {
                                Text(row.detail).font(.caption).foregroundStyle(.secondary)
                            }
                        }
                        Spacer(minLength: 0)
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!enabled)
            }
        }
    }
}

struct DeviceListRow: Identifiable, Hashable, Sendable {
    let addr: String
    let name: String
    let lastConnected: String
    var id: String { addr }

    var title: String { name.isEmpty ? addr : name }

    var detail: String {
        let address = name.isEmpty ? [] : [addr]
        let when = lastConnected.isEmpty ? [] : [lastConnected]
        return (address + when).joined(separator: " · ")
    }
}
