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
    private static let portSettle = Duration.milliseconds(600)
    private static let focusSettle = Duration.milliseconds(400)

    @Bindable var connect: ConnectModel
    @Bindable var sharing: SharingModel

    @State private var discovery = DiscoveryModel()
    @State private var page: DeskhubPage =
        StartPage.index().flatMap(DeskhubPage.init(rawValue:)) ?? .client
    @State private var shareAlert = ""
    @State private var accessibilityWarning = false
    @State private var prompting: DeviceListRow?
    @State private var promptPasscode = ""
    @State private var promptPort = ""
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        HStack(spacing: 0) {
            MainMenuSidebar(page: $page)
            Divider()
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
            discovery.start()
            if sharing.autoShare, !sharing.didAutoShare, !sharing.isSharing, !sharing.isStarting {
                sharing.didAutoShare = true
                if StartPage.index() == nil {
                    page = .host
                }
                await autoShare()
            }
            await sharing.startTenantsIfIdle()
        }
        .task(id: sharing.port) {
            try? await Task.sleep(for: MainMenuView.portSettle)
            guard !Task.isCancelled, sharing.port >= 1, sharing.port <= 65535 else { return }
            discovery.usePort(UInt16(sharing.port))
        }
        .overlay {
            if connect.isConnecting {
                queryingOverlay
            }
        }
        .alert("Deskhub", isPresented: showingConnectError) {
            Button("OK", role: .cancel) { connect.connectError = "" }
        } message: {
            Text(connect.connectError)
        }
        .alert("Deskhub", isPresented: showingShareAlert) {
            if !sharing.hasScreenRecording {
                Button("Grant Screen Recording") { sharing.requestScreenRecording() }
            }
            Button("OK", role: .cancel) {}
        } message: {
            Text(shareAlert)
        }
        .sheet(item: $prompting) { row in
            PasscodePromptSheet(
                address: row.addr,
                port: $promptPort,
                passcode: $promptPasscode,
                onCancel: { prompting = nil },
                onConnect: { confirmPrompt(row) }
            )
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
        .pairingPrompt(sharing.pairing)
    }

    @ViewBuilder
    private func page(for page: DeskhubPage) -> some View {
        switch page {
        case .host: HostPage(sharing: sharing) { Task { await share() } }
        case .client: clientPage
        case .devices: DevicesPage()
        case .settings: SettingsPage(sharing: sharing)
        }
    }

    private var clientPage: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubHeading(DeskhubClient.string(DHStrClientHeading))

            addressForm

            Button(action: beginConnect) {
                Text(DeskhubClient.string(DHStrConnectButton)).deskhubPrimaryLabel()
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .tint(DeskhubPalette.accent)
            .disabled(connect.address.isEmpty || connect.isConnecting)

            deskhubHeadingRow(DeskhubClient.string(DHStrDevicesHeading)) {
                discovery.refreshStatus()
                discovery.rescanNow()
            }
            DeviceTable(
                rows: discovery.devices,
                note: discovery.scanStatus,
                enabled: !connect.isConnecting,
                onPick: pick
            )
        }
    }

    private var addressForm: some View {
        VStack(alignment: .leading, spacing: 10) {
            Grid(alignment: .leading, horizontalSpacing: 12, verticalSpacing: 12) {
                GridRow {
                    Text(DeskhubClient.string(DHStrClientIpPrompt))
                    TextField(
                        DeskhubClient.string(DHStrClientIpPlaceholder), text: $connect.address
                    )
                    .textFieldStyle(.roundedBorder)
                    .frame(width: 260)
                    .onSubmit(beginConnect)
                    .disabled(connect.isConnecting)
                }
                GridRow {
                    Text(DeskhubClient.string(DHStrUdpPortLabel))
                    TextField("", text: $connect.port)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 80)
                        .onSubmit(beginConnect)
                        .disabled(connect.isConnecting)
                }
                GridRow {
                    Text(DeskhubClient.string(DHStrClientPasscodePrompt))
                    PasscodeField(
                        passcode: $connect.passcode,
                        width: 64,
                        enabled: !connect.isConnecting,
                        onSubmit: beginConnect
                    )
                    .help(DeskhubClient.string(DHStrClientPasscodeHint))
                }
                GridRow {
                    Text(DeskhubClient.string(DHStrDeviceNameLabel))
                    TextField("", text: $connect.deviceName)
                        .textFieldStyle(.roundedBorder)
                        .frame(width: 260)
                        .onSubmit(beginConnect)
                        .disabled(connect.isConnecting)
                }
            }
        }
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

    private var queryingOverlay: some View {
        ZStack {
            Color.black.opacity(0.35).ignoresSafeArea()
            VStack(spacing: 16) {
                HStack(spacing: 12) {
                    ProgressView().controlSize(.small)
                    Text(DeskhubClient.string(DHStrQueryingSources))
                }
                Button("Cancel") { connect.forgetHost() }
            }
            .padding(24)
            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
        }
    }
}

extension MainMenuView {
    private func share() async {
        if sharing.isScreenSharing {
            await sharing.stopScreenSharing()
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
        if sharing.isSharing { sharing.stopSharing() }
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
        promptPasscode = DeskhubClient.isValidPasscode(row.passcode)
            ? row.passcode : connect.passcode
        promptPort = DeskhubClient.addressPortText(row.addr)
        prompting = row
    }

    private func confirmPrompt(_ row: DeviceListRow) {
        prompting = nil
        connect.address = DeskhubClient.addressHost(row.addr)
        connect.port = promptPort
        connect.passcode = promptPasscode
        beginConnect()
    }

    private func beginConnect() {
        guard !connect.address.isEmpty, !connect.isConnecting else { return }
        guard connect.acceptAddress() != nil else { return }
        connect.saveDeviceName()
        Task {
            guard let found = await connect.connectAuth() else { return }
            let address = connect.acceptedAddress
            let passcode = connect.acceptedPasscode
            await discovery.remember(address: address, passcode: passcode)
            connect.forgetHost()
            openWindow(value: ConnectionRequest(
                address: address,
                passcode: passcode,
                name: connect.deviceName,
                sources: found.sources,
                caps: found.caps,
                control: sharing.clientControl
            ))
        }
    }
}

@MainActor
func openViewers(_ picked: [Source], address: String, passcode: String, control: Bool,
                 openWindow: OpenWindowAction)
{
    if picked.isEmpty {
        openWindow(value: ViewerRequest(
            address: address, passcode: passcode, sourceId: 0, name: "", control: control
        ))
    } else {
        for source in picked {
            openWindow(value: ViewerRequest(
                address: address, passcode: passcode, sourceId: source.id, name: source.name,
                control: control
            ))
        }
    }
}
