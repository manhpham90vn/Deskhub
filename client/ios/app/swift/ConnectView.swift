import SwiftUI

struct ConnectView: View {
    @Bindable var model: AppModel

    @State private var prompting: DeviceListRow?
    @State private var promptPasscode = ""
    @State private var promptPort = ""

    private var connected: Bool { model.connect.authed != nil }

    private var connectedRow: DeviceListRow? {
        model.discovery.devices.first {
            DeskhubClient.sameDeviceAddr($0.addr, model.connect.acceptedAddress)
        }
    }

    private var liveColor: Color {
        connectedRow?.online == false ? DeskhubPalette.offline : DeskhubPalette.online
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                deskhubHeading(DeskhubClient.string(DHStrClientHeading))

                if connected {
                    connectedHeader
                    sessionButtons

                    Toggle(
                        DeskhubClient.string(DHStrRequestControlLabel),
                        isOn: $model.settings.clientControl
                    )
                    .onChange(of: model.settings.clientControl) { _, _ in model.settings.save() }
                } else {
                    addressFields

                    Button(action: model.beginConnect) {
                        Text(DeskhubClient.string(DHStrConnectButton)).deskhubPrimaryLabel()
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.large)
                    .tint(DeskhubPalette.accent)
                    .disabled(model.connect.address.isEmpty || model.connect.isConnecting)

                    deskhubHeadingRow(DeskhubClient.string(DHStrDevicesHeading)) {
                        model.discovery.refreshStatus()
                        model.discovery.rescanNow()
                    }
                    DeviceListView(
                        heading: "",
                        note: model.discovery.scanStatus,
                        rows: model.discovery.devices,
                        enabled: !model.connect.isConnecting,
                        onPick: pick
                    )
                }
            }
            .padding()
        }
        .overlay {
            if model.connect.isConnecting {
                queryingOverlay
            }
        }
        .alert(
            "Deskhub",
            isPresented: Binding(
                get: { !model.connect.connectError.isEmpty && !model.connect.isConnecting },
                set: { shown in if !shown { model.connect.connectError = "" } }
            )
        ) {
            Button("OK", role: .cancel) { model.connect.connectError = "" }
        } message: {
            Text(model.connect.connectError)
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
        .task { model.discovery.start() }
    }

    private var connectedHeader: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                Text(model.connect.acceptedAddress)
                    .font(.system(size: 17, weight: .semibold))
                    .foregroundStyle(DeskhubPalette.heading)
                    .lineLimit(1)
                    .truncationMode(.middle)
                Spacer(minLength: 0)
                Button(DeskhubClient.string(DHStrDisconnectButton), action: model.dropHost)
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
                if let ping = connectedRow?.ping, !ping.isEmpty {
                    Text(ping)
                        .fontWeight(.semibold)
                        .foregroundStyle(liveColor)
                        .monospacedDigit()
                }
            }
        }
    }

    private var sessionButtons: some View {
        VStack(alignment: .leading, spacing: 16) {
            Button(action: model.openDesktop) {
                Text(DeskhubClient.string(DHStrOpenDesktopLabel)).deskhubPrimaryLabel()
            }
            .buttonStyle(.bordered)
            .controlSize(.large)
            .disabled(!model.connect.canOpenDesktop)

            Button(action: model.openShell) {
                Text(DeskhubClient.string(DHStrOpenShellLabel)).deskhubPrimaryLabel()
            }
            .buttonStyle(.bordered)
            .controlSize(.large)
            .disabled(!model.connect.canOpenShell)

            Button(action: model.openFileSend) {
                Text(DeskhubClient.string(DHStrOpenFilesLabel)).deskhubPrimaryLabel()
            }
            .buttonStyle(.bordered)
            .controlSize(.large)
            .disabled(!model.connect.canOpenFiles)
        }
    }

    private var addressFields: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack(spacing: 12) {
                TextField(
                    DeskhubClient.string(DHStrClientIpPlaceholder),
                    text: $model.connect.address
                )
                .textFieldStyle(.roundedBorder)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .keyboardType(.numbersAndPunctuation)
                .submitLabel(.go)
                .onSubmit(model.beginConnect)

                TextField(
                    DeskhubClient.string(DHStrUdpPortLabel), text: $model.connect.port
                )
                .textFieldStyle(.roundedBorder)
                .keyboardType(.numberPad)
                .frame(width: 90)
            }

            VStack(alignment: .leading, spacing: 4) {
                PasscodeField(
                    passcode: $model.connect.passcode,
                    prompt: DeskhubClient.string(DHStrClientPasscodePrompt)
                )

                Text(DeskhubClient.string(DHStrClientPasscodeHint))
                    .font(.caption)
                    .foregroundStyle(DeskhubPalette.muted)
            }

            TextField("Your name", text: $model.connect.deviceName)
                .textFieldStyle(.roundedBorder)
                .autocorrectionDisabled()
                .submitLabel(.go)
                .onSubmit(model.beginConnect)
        }
    }

    private var queryingOverlay: some View {
        ZStack {
            Color.black.opacity(0.45).ignoresSafeArea()
            VStack(spacing: 16) {
                HStack(spacing: 12) {
                    ProgressView()
                    Text(DeskhubClient.string(DHStrQueryingSources))
                }
                Button("Cancel", action: model.dropHost)
            }
            .padding(24)
            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 16))
        }
    }

    private func pick(_ row: DeviceListRow) {
        promptPasscode = DeskhubClient.isValidPasscode(row.passcode)
            ? row.passcode : model.connect.passcode
        promptPort = DeskhubClient.addressPortText(row.addr)
        prompting = row
    }

    private func confirmPrompt(_ row: DeviceListRow) {
        prompting = nil
        let addr = DeskhubClient.composeAddress(
            DeskhubClient.addressHost(row.addr), portText: promptPort
        )
        model.beginConnect(to: addr, passcode: promptPasscode)
    }
}
