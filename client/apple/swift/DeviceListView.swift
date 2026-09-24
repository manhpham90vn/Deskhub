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
    static let warning = theme(DHThemeWarning)
    static let rowLine = theme(DHThemeRowLine)
    static let viewerRow = theme(DHThemeViewerRow)
    static let panelIdle = theme(DHThemePanelIdle)
    static let panelBusy = theme(DHThemePanelBusy)
    static let panelLive = theme(DHThemePanelLive)
    static let passcodeCard = theme(DHThemePasscodeCard)

    private static func theme(_ color: DHThemeColor) -> Color {
        adaptiveColor(
            light: PaletteRgb(packed: dh_theme_color(color, false)),
            dark: PaletteRgb(packed: dh_theme_color(color, true))
        )
    }
}

enum DeviceRowStyle {
    static func tint(online: Bool?) -> Color {
        switch online {
        case true: DeskhubPalette.online
        case false: DeskhubPalette.offline
        default: DeskhubPalette.heading
        }
    }
}

struct DeviceListView: View {
    let heading: String
    let note: String
    let rows: [DeviceListRow]
    let enabled: Bool
    let onPick: (DeviceListRow) -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if !heading.isEmpty {
                Text(heading).font(.headline)
            }

            ForEach(rows) { row in
                Button {
                    onPick(row)
                } label: {
                    HStack(spacing: 12) {
                        VStack(alignment: .leading, spacing: 1) {
                            Text(row.addr).foregroundStyle(DeviceRowStyle.tint(online: row.online))
                            if !row.detail.isEmpty {
                                Text(row.detail).font(.caption).foregroundStyle(.secondary)
                            }
                        }
                        Spacer(minLength: 0)
                        Text(row.ping)
                            .font(.caption)
                            .foregroundStyle(DeviceRowStyle.tint(online: row.online))
                    }
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                .disabled(!enabled)
            }

            if !note.isEmpty {
                Text(note).font(.caption).foregroundStyle(.secondary)
            }
        }
    }
}

struct DeviceListRow: Identifiable, Hashable, Sendable {
    let addr: String
    let passcode: String
    let origin: String
    let ping: String
    let status: String
    let lastConnected: String
    let online: Bool?
    var id: String { addr }

    var detail: String {
        [origin, status, lastConnected].filter { !$0.isEmpty }.joined(separator: "  ")
    }

    init(addr: String, passcode: String, origin: String, status: String, ping: String,
         lastConnected: String, online: Bool?)
    {
        self.addr = addr
        self.passcode = passcode
        self.origin = origin
        self.status = status
        self.ping = ping
        self.lastConnected = lastConnected
        self.online = online
    }
}
