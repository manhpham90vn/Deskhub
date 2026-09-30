import SwiftUI

struct DevicesPage: View {
    private static let hostFingerprintCapacity = 128
    private static let accessRequestsPollInterval = Duration.seconds(1)

    private static let sectionSpacing: CGFloat = 10
    private static let sectionTopMargin: CGFloat = 8
    private static let itemSpacing: CGFloat = 6
    private static let pairedNameColumnWidth: CGFloat = 200
    private static let pairedKeyColumnWidth: CGFloat = 130
    private static let forgetAllButtonHeight: CGFloat = 46
    private static let confirmYes = "Yes"
    private static let confirmNo = "No"

    let trustedHostsRevision: Int
    let onConnectHost: @MainActor (String) -> Void
    @State private var devices: [PairedDeviceRow] = []
    @State private var hostFingerprint = ""
    @State private var deviceName = ""
    @State private var confirmForgetAll = false
    @State private var publicKeyInput = ""
    @State private var publicKeyError = false
    @State private var accessRequests = AccessRequestsModel()

    var body: some View {
        VStack(alignment: .leading, spacing: Self.sectionSpacing) {
            deskhubHeading(DeskhubClient.string(DHStrSidebarDevices))
            thisMachineSection
                .padding(.top, Self.sectionTopMargin)
            AccessRequestsSection(model: accessRequests)
                .padding(.top, Self.sectionTopMargin)
            pairedSection
                .padding(.top, Self.sectionTopMargin)
            SavedHostsSection(
                trustedHostsRevision: trustedHostsRevision,
                onConnect: onConnectHost
            )
            .padding(.top, Self.sectionTopMargin)
        }
        .onAppear(perform: refresh)
        .task { await pollAccessRequests() }
        .onChange(of: accessRequests.requests.map(\.fingerprint)) { _, _ in refresh() }
        .alert("Deskhub", isPresented: $confirmForgetAll) {
            forgetAllButtons
        } message: {
            Text(DeskhubClient.string(DHStrPairedForgetAllPrompt))
        }
    }

    @ViewBuilder
    private var forgetAllButtons: some View {
        Button(Self.confirmYes, role: .destructive, action: forgetAll)
        Button(Self.confirmNo, role: .cancel) {}
            .keyboardShortcut(.defaultAction)
    }

    private var hasHostIdentity: Bool { !hostFingerprint.isEmpty }

    private var hostFingerprintText: String {
        hasHostIdentity ? hostFingerprint : DeskhubClient.string(DHStrShareNoHostIdentity)
    }

    private var thisMachineSection: some View {
        VStack(alignment: .leading, spacing: Self.itemSpacing) {
            deskhubSection(DeskhubClient.string(DHStrThisMachineHeading))
            deskhubHint(DeskhubClient.string(DHStrThisMachineHint))
            HStack(spacing: 10) {
                Text(DeskhubClient.string(DHStrDeviceNameLabel))
                    .foregroundStyle(DeskhubPalette.muted)
                Text(deviceName)
                    .foregroundStyle(DeskhubPalette.heading)
                    .textSelection(.enabled)
            }
            HStack(spacing: 10) {
                Text(hostFingerprintText)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .textSelection(.enabled)
                CopyButton { hostFingerprint }
                    .buttonStyle(.bordered)
                    .disabled(!hasHostIdentity)
                CopyButton(DeskhubClient.string(DHStrCopyPublicKeyAction)) {
                    DeskhubClient.hostPublicKey()
                }
                .buttonStyle(.bordered)
                .disabled(!hasHostIdentity)
            }
        }
    }

    private var pairedSection: some View {
        VStack(alignment: .leading, spacing: Self.itemSpacing) {
            deskhubSection(DeskhubClient.string(DHStrPairedHeading))
            deskhubHint(DeskhubClient.string(DHStrPairedHint))
            allowClientForm
            pairedList
            deskhubHint(DeskhubClient.string(DHStrPairedForgetNote))
        }
    }

    private var allowClientForm: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 10) {
                TextField(
                    DeskhubClient.string(DHStrAllowClientPlaceholder), text: $publicKeyInput
                )
                .textFieldStyle(.roundedBorder)
                .autocorrectionDisabled()
                .onSubmit(allowClient)
                Button(DeskhubClient.string(DHStrAllowClientAction), action: allowClient)
                    .buttonStyle(.bordered)
            }
            if publicKeyError {
                Text(DeskhubClient.string(DHStrAllowClientInvalid))
                    .foregroundStyle(DeskhubPalette.errorText)
                    .fixedSize(horizontal: false, vertical: true)
            }
        }
    }

    @ViewBuilder
    private var pairedList: some View {
        DeskhubListFrame {
            HStack(spacing: DeskhubListMetrics.columnGap) {
                deskhubListHeaderCell(
                    DeskhubClient.string(DHStrPairedColumnName),
                    width: Self.pairedNameColumnWidth
                )
                deskhubListHeaderCell(
                    DeskhubClient.string(DHStrPairedColumnKey),
                    width: Self.pairedKeyColumnWidth
                )
            }
            ForEach(devices) { device in
                HStack(spacing: DeskhubListMetrics.columnGap) {
                    deskhubListCell(
                        device.name.isEmpty ? DeskhubClient.string(DHStrUnnamedClient) : device.name,
                        width: Self.pairedNameColumnWidth
                    )
                    deskhubListCell(device.shortKey, width: Self.pairedKeyColumnWidth)
                    DeskhubRowActionButton(
                        title: DeskhubClient.string(DHStrPairedForget),
                        tint: DeskhubPalette.offline
                    ) {
                        forget(device)
                    }
                }
            }
        }

        if devices.isEmpty {
            deskhubHint(DeskhubClient.string(DHStrPairedEmpty))
        }

        Button {
            if !devices.isEmpty { confirmForgetAll = true }
        } label: {
            Text(DeskhubClient.string(DHStrPairedForgetAll))
                .frame(minHeight: Self.forgetAllButtonHeight)
        }
        .buttonStyle(.bordered)
    }

    private var submittedPublicKey: String {
        publicKeyInput.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    private func allowClient() {
        let key = submittedPublicKey
        guard !key.isEmpty, dh_paired_add_public_key(key) else {
            publicKeyError = true
            return
        }
        publicKeyInput = ""
        publicKeyError = false
        refresh()
    }

    private func forget(_ device: PairedDeviceRow) {
        _ = dh_paired_forget(device.fingerprint)
        refresh()
    }

    private func forgetAll() {
        dh_paired_forget_all()
        refresh()
    }

    @MainActor
    private func pollAccessRequests() async {
        while !Task.isCancelled {
            accessRequests.refresh()
            try? await Task.sleep(for: Self.accessRequestsPollInterval)
        }
    }

    private func refresh() {
        deviceName = DeviceNameModel.sessionName
        hostFingerprint = DeskhubClient.buffered(Self.hostFingerprintCapacity) {
            dh_host_fingerprint($0, $1)
        }
        devices = DeskhubClient.ffiList(
            128, DHPairedDevice(),
            { dh_paired_devices($0, $1) },
            { raw in
                PairedDeviceRow(
                    name: DeskhubClient.cString(raw.name),
                    shortKey: DeskhubClient.cString(raw.shortKey),
                    fingerprint: DeskhubClient.cString(raw.fingerprint)
                )
            }
        )
    }
}
