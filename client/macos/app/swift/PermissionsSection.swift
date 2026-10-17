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
                state: sharing.hasScreenRecording ? .granted : .denied,
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
                state: sharing.hasAccessibility ? .granted : .denied,
                detail: "Needed for the other machine to move the mouse and type.",
                grant: { sharing.requestAccessibility() },
                openSettings: DeskhubShare.openAccessibilitySettings
            )

            UnattendedSection(sharing: sharing)
        }
        .onReceive(
            NotificationCenter.default.publisher(for: NSApplication.didBecomeActiveNotification)
        ) { _ in
            sharing.refreshPermissions()
        }
    }
}

private struct UnattendedSection: View {
    @Bindable var sharing: SharingModel

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Toggle(DeskhubClient.string(DHStrUnattendedLabel), isOn: unattendedBinding)
                .toggleStyle(.checkbox)
                .disabled(!sharing.unattendedAvailable)
            Text(sharing.unattendedNote)
                .foregroundStyle(DeskhubPalette.muted)
                .fixedSize(horizontal: false, vertical: true)
            if sharing.unattendedNeedsApproval {
                Button(DeskhubClient.string(DHStrUnattendedOpenLoginItems)) {
                    DeskhubShare.openLoginItemsSettings()
                }
            }
            if sharing.unattended {
                hostPermissions
            }
        }
        .padding(.top, 6)
        .onAppear { sharing.refreshUnattended() }
    }

    private var unattendedBinding: Binding<Bool> {
        Binding(
            get: { sharing.unattended },
            set: { sharing.setUnattended($0) }
        )
    }

    private var hostPermissions: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(DeskhubClient.string(DHStrUnattendedHostPermissionsHint))
                .foregroundStyle(DeskhubPalette.muted)
                .fixedSize(horizontal: false, vertical: true)
            PermissionRow(
                title: DeskhubClient.string(DHStrUnattendedHostScreenLabel),
                state: hostState(sharing.unattendedPermissions.screenRecording),
                detail: "",
                grant: nil,
                openSettings: DeskhubShare.openScreenRecordingSettings
            )
            PermissionRow(
                title: DeskhubClient.string(DHStrUnattendedHostInputLabel),
                state: hostState(sharing.unattendedPermissions.accessibility),
                detail: "",
                grant: nil,
                openSettings: DeskhubShare.openAccessibilitySettings
            )
        }
    }

    private func hostState(_ granted: Bool) -> PermissionState {
        guard sharing.unattendedPermissions.known else { return .unknown }
        return granted ? .granted : .denied
    }
}

private enum PermissionState {
    case granted
    case denied
    case unknown

    var label: String {
        switch self {
        case .granted: "Granted"
        case .denied: "Not granted"
        case .unknown: DeskhubClient.string(DHStrUnattendedPermissionUnknown)
        }
    }

    var color: Color {
        switch self {
        case .granted: DeskhubPalette.online
        case .denied: Color.red
        case .unknown: DeskhubPalette.muted
        }
    }
}

private struct PermissionRow: View {
    let title: String
    let state: PermissionState
    let detail: String
    let grant: (() -> Void)?
    let openSettings: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 10) {
                Text(title).frame(width: 220, alignment: .leading)
                Text(state.label)
                    .fontWeight(.bold)
                    .foregroundStyle(state.color)
                Spacer(minLength: 0)
                if state != .granted, let grant {
                    Button("Grant", action: grant)
                }
                Button("Open Settings", action: openSettings)
            }
            if !detail.isEmpty {
                Text(detail)
                    .foregroundStyle(DeskhubPalette.muted)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }
}
