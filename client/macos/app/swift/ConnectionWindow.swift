import SwiftUI

struct ConnectionRequest: Codable, Hashable {
    var address: String
    var passcode: String
    var name: String
    var sources: [Source]
    var caps: HostCaps
    var control: Bool
}

struct ConnectionWindow: View {
    private static let pollInterval = Duration.seconds(1)

    let request: ConnectionRequest

    @State private var control: Bool
    @State private var ping = ""
    @State private var online: Bool?
    @State private var picking = false
    @Environment(\.openWindow) private var openWindow
    @Environment(\.dismiss) private var dismiss

    init(request: ConnectionRequest) {
        self.request = request
        _control = State(initialValue: request.control)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            header
            sessionChoices
            Spacer(minLength: 0)
        }
        .padding(16)
        .frame(minWidth: 380, idealWidth: 460, minHeight: 260)
        .navigationTitle(request.address)
        .sheet(isPresented: $picking) {
            SourcePickerView(
                sources: request.sources,
                onCancel: { picking = false },
                onPick: { chosen in
                    picking = false
                    openViewers(chosen, address: request.address, passcode: request.passcode,
                                control: control, openWindow: openWindow)
                }
            )
            .frame(width: 460, height: 340)
        }
        .task {
            while !Task.isCancelled {
                let row = await Task.detached { [address = request.address] in
                    DeskhubDiscovery.deviceRows().first {
                        DeskhubClient.sameDeviceAddr($0.addr, address)
                    }
                }.value
                ping = row?.ping ?? ""
                online = row?.online
                try? await Task.sleep(for: ConnectionWindow.pollInterval)
            }
        }
    }

    private var liveColor: Color {
        online == false ? DeskhubPalette.offline : DeskhubPalette.online
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 12) {
                Text(request.address)
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(DeskhubPalette.heading)
                    .lineLimit(1)
                    .truncationMode(.middle)
                Spacer(minLength: 0)
                Button(DeskhubClient.string(DHStrDisconnectButton)) { dismiss() }
                    .buttonStyle(.borderedProminent)
                    .tint(DeskhubPalette.accent)
            }

            HStack(spacing: 8) {
                Circle()
                    .fill(liveColor)
                    .frame(width: 10, height: 10)
                Text(DeskhubClient.string(DHStrConnectedPickSession))
                    .fontWeight(.semibold)
                    .foregroundStyle(liveColor)
                Spacer(minLength: 0)
                if !ping.isEmpty {
                    Text(ping)
                        .fontWeight(.semibold)
                        .foregroundStyle(liveColor)
                        .monospacedDigit()
                }
            }
        }
    }

    private var sessionChoices: some View {
        VStack(alignment: .leading, spacing: 8) {
            Button(action: openDesktopSession) {
                Text(DeskhubClient.string(DHStrOpenDesktopLabel))
            }
            .disabled(request.sources.isEmpty)
            Toggle(DeskhubClient.string(DHStrRequestControlLabel), isOn: $control)
                .toggleStyle(.checkbox)
                .padding(.leading, 24)
                .disabled(request.sources.isEmpty)
                .onChange(of: control) { _, on in
                    var stored = dh_settings_load()
                    stored.clientControl = on
                    dh_settings_save(stored.fps, stored.bitrateMbps, stored.maxDim, stored.port,
                                     stored.allowInput, on, nil)
                }
            Button(action: openTerminalSession) {
                Text(DeskhubClient.string(DHStrOpenShellLabel))
            }
            .disabled(!request.caps.terminal)
            Button(action: openFilesSession) {
                Text(DeskhubClient.string(DHStrOpenFilesLabel))
            }
            .disabled(!request.caps.files)
            deskhubHint(DeskhubClient.string(DHStrMobileHostNote))
        }
    }

    private func openDesktopSession() {
        guard !request.sources.isEmpty else { return }
        let decision = DeskhubClient.connectDecision(request.sources)
        if decision.showPicker {
            picking = true
        } else {
            openViewers(request.sources, address: request.address, passcode: request.passcode,
                        control: control, openWindow: openWindow)
        }
    }

    private func openTerminalSession() {
        guard request.caps.terminal else { return }
        openWindow(value: TerminalRequest(
            address: request.address, passcode: request.passcode
        ))
    }

    private func openFilesSession() {
        guard request.caps.files else { return }
        openWindow(value: TransferRequest(
            address: request.address, passcode: request.passcode, name: request.name
        ))
    }
}
