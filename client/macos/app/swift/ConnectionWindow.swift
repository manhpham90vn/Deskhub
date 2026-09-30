import SwiftUI

struct ConnectionRequest: Codable, Hashable {
    var address: String
    var sources: [Source]
    var caps: HostCaps
    var control: Bool
}

struct ConnectionWindow: View {
    let request: ConnectionRequest

    @State private var control: Bool
    @State private var picking = false
    @Environment(\.openWindow) private var openWindow
    @Environment(\.dismiss) private var dismiss

    init(request: ConnectionRequest) {
        self.request = request
        _control = State(initialValue: request.control)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            addressRow
            stateRow
            Button(action: openDesktopSession) {
                Text(DeskhubClient.string(DHStrOpenDesktopLabel))
                    .frame(maxWidth: .infinity)
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
                                     stored.allowInput, on)
                }
            Button(action: openTerminalSession) {
                Text(DeskhubClient.string(DHStrOpenShellLabel))
                    .frame(maxWidth: .infinity)
            }
            .disabled(!request.caps.terminal)
            Button(action: openFilesSession) {
                Text(DeskhubClient.string(DHStrOpenFilesLabel))
                    .frame(maxWidth: .infinity)
            }
            .disabled(!request.caps.files)
            deskhubHint(DeskhubClient.string(DHStrMobileHostNote))
        }
        .padding(16)
        .frame(idealWidth: 460, alignment: .topLeading)
        .navigationTitle(request.address)
        .sheet(isPresented: $picking) {
            SourcePickerView(
                sources: request.sources,
                onCancel: { picking = false },
                onPick: { chosen in
                    picking = false
                    openViewers(chosen, address: request.address, control: control,
                                openWindow: openWindow)
                }
            )
            .frame(width: 460, height: 340)
        }
    }

    private var addressRow: some View {
        HStack(spacing: 12) {
            Text(request.address)
                .font(.system(size: 15, weight: .bold))
                .foregroundStyle(DeskhubPalette.heading)
                .fixedSize(horizontal: false, vertical: true)
            Spacer(minLength: 0)
            Button {
                dismiss()
            } label: {
                Text(DeskhubClient.string(DHStrDisconnectButton)).fontWeight(.bold)
            }
            .buttonStyle(.borderedProminent)
            .tint(DeskhubPalette.offline)
        }
    }

    private var stateRow: some View {
        HStack(spacing: 8) {
            statusPill(DeskhubClient.string(DHStrConnectedPickSession))
            statusPill("")
        }
    }

    private func statusPill(_ text: String) -> some View {
        Text(text)
            .fontWeight(.bold)
            .foregroundStyle(DeskhubPalette.onlineText)
            .padding(.vertical, 2)
            .padding(.horizontal, 6)
            .background(RoundedRectangle(cornerRadius: 4).fill(DeskhubPalette.panelLive))
    }

    private func openDesktopSession() {
        guard !request.sources.isEmpty else { return }
        let decision = DeskhubClient.connectDecision(request.sources)
        if decision.showPicker {
            picking = true
        } else {
            openViewers(request.sources, address: request.address, control: control,
                        openWindow: openWindow)
        }
    }

    private func openTerminalSession() {
        guard request.caps.terminal else { return }
        openWindow(value: TerminalRequest(address: request.address))
    }

    private func openFilesSession() {
        guard request.caps.files else { return }
        openWindow(value: TransferRequest(address: request.address))
    }
}
