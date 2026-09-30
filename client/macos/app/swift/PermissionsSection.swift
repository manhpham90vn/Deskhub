import AppKit
import SwiftUI

struct PermissionsSection: View {
    @Bindable var sharing: SharingModel

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubSection("Permissions")
                .frame(maxWidth: .infinity, alignment: .leading)

            PermissionRow(
                title: "Screen Recording",
                granted: sharing.hasScreenRecording,
                detail: "Needed to share this Mac's display.",
                grant: { sharing.requestScreenRecording() },
                openSettings: DeskhubShare.openScreenRecordingSettings
            )

            if sharing.screenRecordingNeedsRelaunch {
                Text("Deskhub is now listed under Privacy & Security \u{2192} Screen & System "
                    + "Audio Recording. Turn it on there, then quit and reopen Deskhub.")
                    .foregroundStyle(DeskhubPalette.muted)
                    .fixedSize(horizontal: false, vertical: true)
            }

            PermissionRow(
                title: "Accessibility",
                granted: sharing.hasAccessibility,
                detail: "Needed for the other machine to move the mouse and type.",
                grant: { sharing.requestAccessibility() },
                openSettings: DeskhubShare.openAccessibilitySettings
            )
        }
        .onReceive(
            NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)
        ) { _ in
            sharing.refreshPermissions()
        }
    }
}

private struct PermissionRow: View {
    let title: String
    let granted: Bool
    let detail: String
    let grant: () -> Void
    let openSettings: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 10) {
                Text(title).frame(width: 140, alignment: .leading)
                Text(granted ? "Granted" : "Not granted")
                    .fontWeight(.bold)
                    .foregroundStyle(granted ? DeskhubPalette.online : Color.red)
                Spacer(minLength: 0)
                if !granted {
                    Button("Grant", action: grant)
                }
                Button("Open Settings", action: openSettings)
            }
            Text(detail)
                .foregroundStyle(DeskhubPalette.muted)
                .fixedSize(horizontal: false, vertical: true)
        }
    }
}
