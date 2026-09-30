import SwiftUI

struct ConnectView: View {
    private static let recentPollInterval = Duration.seconds(1)
    private static let portFieldWidth: CGFloat = 110
    private static let queryingCardWidth: CGFloat = 270

    @Bindable var model: AppModel
    @State private var scanning = false
    @State private var cameraDenied = false

    private var connected: Bool { model.connect.authed != nil }
    private var busy: Bool { model.connect.isConnecting }

    private var addressReady: Bool {
        !model.connect.address.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty && !busy
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: iosPageSpacing) {
                iosHeading(DeskhubClient.string(DHStrClientHeading))

                if connected {
                    connectedHeader
                    sessionButtons
                } else {
                    addressFields
                    connectButtons
                    iosHeading(DeskhubClient.string(DHStrDevicesHeading))
                    DeviceListView(
                        rows: model.recent.devices,
                        enabled: !busy,
                        onPick: pick
                    )
                    .foregroundStyle(DeskhubPalette.muted)
                }
            }
            .padding(iosPagePadding)
        }
        .overlay {
            if busy {
                queryingDialog
            }
        }
        .alert(
            "Deskhub",
            isPresented: Binding(
                get: { !model.connect.connectError.isEmpty && !busy },
                set: { shown in if !shown { model.connect.connectError = "" } }
            )
        ) {
            Button("OK", role: .cancel) { model.connect.connectError = "" }
        } message: {
            Text(model.connect.connectError)
        }
        .hostTrustAlert(model.connect) { model.beginConnect(to: $0) }
        .fullScreenCover(isPresented: $scanning) {
            QrScannerView { model.beginConnect(invite: $0) }
        }
        .task { await pollRecent() }
    }

    private var connectedHeader: some View {
        VStack(alignment: .leading, spacing: iosPageSpacing) {
            HStack(spacing: 12) {
                Text(model.connect.acceptedAddress)
                    .font(.system(size: iosSectionSize, weight: .medium))
                    .foregroundStyle(DeskhubPalette.heading)
                    .frame(maxWidth: .infinity, alignment: .leading)
                Button(DeskhubClient.string(DHStrDisconnectButton), action: model.dropHost)
                    .buttonStyle(.iosFilled())
            }

            HStack(spacing: 8) {
                Circle()
                    .fill(DeskhubPalette.online)
                    .frame(width: 10, height: 10)
                Text(DeskhubClient.string(DHStrConnectedPickSession))
                    .fontWeight(.semibold)
                    .foregroundStyle(DeskhubPalette.online)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
    }

    private var sessionButtons: some View {
        VStack(alignment: .leading, spacing: iosPageSpacing) {
            Button(DeskhubClient.string(DHStrOpenDesktopLabel), action: model.openDesktop)
                .buttonStyle(.iosOutlined(fullWidth: true))
                .disabled(!model.connect.canOpenDesktop)

            Toggle(
                DeskhubClient.string(DHStrRequestControlLabel),
                isOn: $model.settings.clientControl
            )
            .tint(DeskhubPalette.accent)
            .onChange(of: model.settings.clientControl) { _, _ in model.settings.save() }

            Button(DeskhubClient.string(DHStrOpenShellLabel), action: model.openShell)
                .buttonStyle(.iosOutlined(fullWidth: true))
                .disabled(!model.connect.canOpenShell)

            Button(DeskhubClient.string(DHStrOpenFilesLabel), action: model.openFileSend)
                .buttonStyle(.iosOutlined(fullWidth: true))
                .disabled(!model.connect.canOpenFiles)
        }
    }

    private var addressFields: some View {
        HStack(alignment: .top, spacing: 12) {
            TextField(
                DeskhubClient.string(DHStrClientIpPrompt),
                text: $model.connect.address
            )
            .textInputAutocapitalization(.never)
            .autocorrectionDisabled()
            .keyboardType(.numbersAndPunctuation)
            .submitLabel(.go)
            .onSubmit(model.beginConnect)
            .iosFieldBorder()

            TextField(DeskhubClient.string(DHStrUdpPortLabel), text: $model.connect.port)
                .keyboardType(.numberPad)
                .submitLabel(.go)
                .onSubmit(model.beginConnect)
                .onChange(of: model.connect.port) { _, typed in
                    let limited = DigitsOnly.limit(typed, to: DigitsOnly.portLength)
                    if limited != typed { model.connect.port = limited }
                }
                .iosFieldBorder()
                .frame(width: ConnectView.portFieldWidth)
        }
        .disabled(busy)
    }

    private var connectButtons: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 12) {
                Button(DeskhubClient.string(DHStrConnectButton), action: model.beginConnect)
                    .buttonStyle(.iosFilled(fullWidth: true))
                    .disabled(!addressReady)
                Button(DeskhubClient.string(DHStrScanQrAction), action: scanQr)
                    .buttonStyle(.iosOutlined())
                    .disabled(busy)
            }
            if cameraDenied {
                iosError(DeskhubClient.string(DHStrCameraDenied))
            }
        }
    }

    private var queryingDialog: some View {
        ZStack {
            Color.black.opacity(0.45)
                .ignoresSafeArea()
                .onTapGesture(perform: model.cancelConnect)
            VStack(alignment: .leading, spacing: 12) {
                HStack(spacing: 12) {
                    ProgressView()
                    Text(DeskhubClient.string(DHStrQueryingSources))
                }
                if !model.connect.waitingStatus.isEmpty {
                    iosNote(model.connect.waitingStatus)
                }
                HStack {
                    Spacer(minLength: 0)
                    Button(DeskhubClient.string(DHStrCancelAction), action: model.cancelConnect)
                        .buttonStyle(.iosText())
                }
            }
            .padding(20)
            .frame(width: ConnectView.queryingCardWidth)
            .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 14))
        }
    }

    private func scanQr() {
        Task {
            let granted = await CameraAccess.request()
            cameraDenied = !granted
            if granted { scanning = true }
        }
    }

    private func pollRecent() async {
        while !Task.isCancelled {
            await model.recent.reload()
            try? await Task.sleep(for: ConnectView.recentPollInterval)
        }
    }

    private func pick(_ row: DeviceListRow) {
        model.beginConnect(to: row.addr)
    }
}
