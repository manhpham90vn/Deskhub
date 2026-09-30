import AppKit
import SwiftUI

enum DeskhubPage: Int, CaseIterable, Identifiable {
    case host
    case client
    case devices
    case settings

    var id: Int { rawValue }

    var label: String {
        switch self {
        case .host: DeskhubClient.string(DHStrSidebarHost)
        case .client: DeskhubClient.string(DHStrSidebarClient)
        case .devices: DeskhubClient.string(DHStrSidebarDevices)
        case .settings: DeskhubClient.string(DHStrSidebarSettings)
        }
    }
}

struct MainMenuView: View {
    private static let focusSettle = Duration.milliseconds(400)
    private static let emptyAddressWarning =
        "Enter the host machine's IP address first (e.g., 192.168.1.10)."

    @Bindable var connect: ConnectModel
    @Bindable var sharing: SharingModel

    @State private var recent = RecentDevicesModel()
    @State private var page: DeskhubPage =
        StartPage.index().flatMap(DeskhubPage.init(rawValue:)) ?? .client
    @State private var shareAlert = ""
    @State private var accessibilityWarning = false
    @State private var addressMissing = false
    @State private var clientStatusError = ""
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        HStack(spacing: 0) {
            MainMenuSidebar(page: $page)
            ScrollView {
                page(for: page).padding(16)
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(nsColor: .textBackgroundColor))
        }
        .task {
            guard StartPage.index() != nil else { return }
            try? await Task.sleep(for: MainMenuView.focusSettle)
            NSApp.keyWindow?.makeFirstResponder(nil)
        }
        .task {
            sharing.refreshPermissions()
            sharing.loadAddresses()
            recent.refresh()
            if sharing.autoShare, !sharing.didAutoShare, !sharing.isSharing, !sharing.isStarting {
                sharing.didAutoShare = true
                if StartPage.index() == nil {
                    page = .host
                }
                await autoShare()
            }
        }
        .hostTrustAlert(connect) { beginConnect(to: $0) }
        .alert("Deskhub", isPresented: showingConnectError) {
            Button("OK", role: .cancel) { connect.connectError = "" }
        } message: {
            Text(connect.connectError)
        }
        .alert("Deskhub", isPresented: $addressMissing) {
            Button("OK", role: .cancel) {}
        } message: {
            Text(MainMenuView.emptyAddressWarning)
        }
        .alert("Deskhub", isPresented: showingShareAlert) {
            if !sharing.hasScreenRecording {
                Button("Grant Screen Recording") { sharing.requestScreenRecording() }
            }
            Button("OK", role: .cancel) {}
        } message: {
            Text(shareAlert)
        }
        .alert("Deskhub", isPresented: $accessibilityWarning) {
            Button("Share anyway") { Task { await doShare() } }
            Button("Grant Accessibility", role: .cancel) {
                sharing.requestAccessibility()
            }
        } message: {
            Text("Mouse and keyboard are always shared, but macOS silently drops "
                + "them until Deskhub has Accessibility permission. The other "
                + "machine will see this Mac but not control it.")
        }
    }

    @ViewBuilder
    private func page(for page: DeskhubPage) -> some View {
        switch page {
        case .host: HostPage(sharing: sharing) { Task { await share() } }
        case .client: clientPage
        case .devices:
            DevicesPage(
                trustedHostsRevision: connect.trustedHostsRevision,
                onConnectHost: beginConnect(to:)
            )
        case .settings: SettingsPage(sharing: sharing)
        }
    }

    private var clientPage: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubHeading(DeskhubClient.string(DHStrClientHeading))

            addressForm

            Button(action: beginConnect) {
                Text(DeskhubClient.string(DHStrConnectButton))
            }
            .buttonStyle(DeskhubPrimaryButtonStyle(fill: DeskhubPalette.accent))
            .disabled(connect.isConnecting)

            statusRow

            deskhubHeading(DeskhubClient.string(DHStrDevicesHeading))
            DeviceTable(rows: recent.devices, onPick: pick)
            deskhubHint(DeskhubClient.string(DHStrLanDevicesEmpty))
        }
    }

    private var addressForm: some View {
        Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 12) {
            GridRow {
                Text(DeskhubClient.string(DHStrClientIpPrompt))
                TextField(
                    DeskhubClient.string(DHStrClientIpPlaceholder), text: $connect.address
                )
                .textFieldStyle(.roundedBorder)
                .frame(width: DeskhubControlMetrics.addressFieldWidth)
                .onSubmit(beginConnect)
            }
            GridRow {
                Text(DeskhubClient.string(DHStrUdpPortLabel))
                TextField("", text: $connect.port)
                    .textFieldStyle(.roundedBorder)
                    .frame(width: DeskhubControlMetrics.portFieldWidth)
                    .onSubmit(beginConnect)
            }
        }
    }

    private var statusRow: some View {
        HStack(alignment: .top, spacing: 10) {
            Text(clientStatusText)
                .foregroundStyle(clientStatusIsError ? DeskhubPalette.offline : DeskhubPalette.muted)
                .fixedSize(horizontal: false, vertical: true)
                .frame(maxWidth: .infinity, alignment: .leading)
            if connect.isConnecting, connect.progressReported {
                Button(DeskhubClient.string(DHStrCancelAction)) { connect.requestCancel() }
                    .buttonStyle(.bordered)
                    .disabled(connect.cancelRequested)
            }
        }
    }

    private var clientStatusText: String {
        guard connect.isConnecting else { return clientStatusError }
        return connect.waitingStatus.isEmpty
            ? DeskhubClient.string(DHStrQueryingSources)
            : connect.waitingStatus
    }

    private var clientStatusIsError: Bool {
        !connect.isConnecting && !clientStatusError.isEmpty
    }

    private var showingShareAlert: Binding<Bool> {
        Binding(get: { !shareAlert.isEmpty }, set: { if !$0 { shareAlert = "" } })
    }

    private var showingConnectError: Binding<Bool> {
        Binding(
            get: { !connect.connectError.isEmpty && !connect.isConnecting },
            set: { if !$0 { connect.connectError = "" } }
        )
    }
}

extension MainMenuView {
    private func share() async {
        if sharing.isSharing {
            sharing.stopSharing()
            return
        }
        sharing.refreshPermissions()
        let tenantsOnly = sharing.pickedSources.isEmpty
            && (sharing.shareTerminal || sharing.shareFiles)
        if !tenantsOnly, !sharing.hasScreenRecording {
            shareAlert = DeskhubClient.string(DHStrScreenRecordingRequired)
            return
        }
        if !tenantsOnly, !sharing.hasAccessibility {
            accessibilityWarning = true
            return
        }
        await doShare()
    }

    private func autoShare() async {
        sharing.refreshPermissions()
        guard sharing.hasScreenRecording else { return }
        guard await sharing.waitForShareSources() else { return }
        _ = await sharing.startSharing()
    }

    private func doShare() async {
        guard await sharing.startSharing() else {
            shareAlert = sharing.startError
            return
        }
        if !sharing.clampWarning.isEmpty {
            shareAlert = sharing.clampWarning
            sharing.clampWarning = ""
        }
    }

    private func pick(_ row: DeviceListRow) {
        beginConnect(to: row.addr)
    }

    private func beginConnect(to address: String) {
        connect.target(address)
        beginConnect()
    }

    private func beginConnect() {
        let typed = connect.address.trimmingCharacters(in: .whitespacesAndNewlines)
        if typed.isEmpty {
            addressMissing = true
            return
        }
        guard !connect.isConnecting else { return }
        clientStatusError = ""
        if DeskhubClient.isPairingInvite(typed), DeskhubClient.pairingInviteAddress(typed).isEmpty {
            clientStatusError = DeskhubClient.string(DHStrInviteInvalid)
            return
        }
        Task {
            guard let found = await connect.connectAuth() else { return }
            let address = connect.acceptedAddress
            await recent.reload()
            connect.forgetHost()
            openWindow(value: ConnectionRequest(
                address: address,
                sources: found.sources,
                caps: found.caps,
                control: sharing.clientControl
            ))
        }
    }
}

@MainActor
func openViewers(_ picked: [Source], address: String, control: Bool,
                 openWindow: OpenWindowAction)
{
    if picked.isEmpty {
        openWindow(value: ViewerRequest(
            address: address, sourceId: 0, name: "", control: control
        ))
    } else {
        for source in picked {
            openWindow(value: ViewerRequest(
                address: address, sourceId: source.id, name: source.name, control: control
            ))
        }
    }
}
