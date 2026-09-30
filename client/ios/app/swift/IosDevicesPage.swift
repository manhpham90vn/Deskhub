import SwiftUI

extension SavedHostsModel {
    func removeReportingFailure(_ host: SavedHostRow) -> String {
        let result = dh_host_profile_remove(host.alias)
        refresh()
        guard result != DHHostProfileOk else { return "" }
        return String(cString: dh_host_profile_error_text(result))
    }
}

struct IosDevicesPage: View {
    private static let hostFingerprintCapacity = 128
    private static let pairedCapacity = 128
    private static let accessRequestsPollInterval = Duration.seconds(1)

    let trustedHostsRevision: Int
    let onConnectHost: @MainActor (String) -> Void

    @State private var devices: [PairedDeviceRow] = []
    @State private var hostFingerprint = ""
    @State private var deviceName = ""
    @State private var confirmForgetAll = false
    @State private var publicKeyInput = ""
    @State private var publicKeyInvalid = false
    @State private var accessRequests = AccessRequestsModel()
    @State private var savedHosts = SavedHostsModel()
    @State private var removeHostError = ""

    private var trimmedPublicKey: String {
        publicKeyInput.trimmingCharacters(in: .whitespacesAndNewlines)
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: iosPageSpacing) {
                thisMachineSection
                IosAccessRequestsSection(model: accessRequests)
                allowedClientsSection
                savedHostsSection
            }
            .padding(iosPagePadding)
        }
        .onAppear(perform: refresh)
        .task { await pollAccessRequests() }
        .onChange(of: accessRequests.requests.map(\.fingerprint)) { _, _ in refreshPaired() }
        .onChange(of: trustedHostsRevision) { _, _ in savedHosts.refresh() }
        .alert(DeskhubClient.string(DHStrPairedForgetAll), isPresented: $confirmForgetAll) {
            Button(DeskhubClient.string(DHStrPairedForgetAll), role: .destructive) {
                dh_paired_forget_all()
                refreshPaired()
            }
            Button(DeskhubClient.string(DHStrCancelAction), role: .cancel) {}
        } message: {
            Text(DeskhubClient.string(DHStrPairedForgetAllPrompt))
        }
    }

    @ViewBuilder private var thisMachineSection: some View {
        iosSection(DeskhubClient.string(DHStrThisMachineHeading))
        iosNote(DeskhubClient.string(DHStrThisMachineHint))
        HStack(spacing: 8) {
            iosHint(DeskhubClient.string(DHStrDeviceNameLabel))
            Text(deviceName)
                .foregroundStyle(DeskhubPalette.heading)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        HStack(spacing: 8) {
            Text(hostFingerprint)
                .font(.system(size: iosHintSize, design: .monospaced))
                .foregroundStyle(DeskhubPalette.heading)
                .textSelection(.enabled)
                .frame(maxWidth: .infinity, alignment: .leading)
            Button(DeskhubClient.string(DHStrCopyButton)) {
                DeskhubPasteboard.copy(hostFingerprint)
            }
            .buttonStyle(.iosText())
        }
        Button(DeskhubClient.string(DHStrCopyPublicKeyAction)) {
            DeskhubPasteboard.copy(DeskhubClient.hostPublicKey())
        }
        .buttonStyle(.iosOutlined())
    }

    @ViewBuilder private var allowedClientsSection: some View {
        iosSection(DeskhubClient.string(DHStrPairedHeading))
        iosNote(DeskhubClient.string(DHStrPairedHint))
        HStack(spacing: 8) {
            TextField(DeskhubClient.string(DHStrAllowClientPlaceholder), text: $publicKeyInput)
                .textInputAutocapitalization(.never)
                .autocorrectionDisabled()
                .onChange(of: publicKeyInput) { _, _ in publicKeyInvalid = false }
                .onSubmit(allowClient)
                .iosFieldBorder()
            Button(DeskhubClient.string(DHStrAllowClientAction), action: allowClient)
                .buttonStyle(.iosFilled())
                .disabled(trimmedPublicKey.isEmpty)
        }
        if publicKeyInvalid {
            iosError(DeskhubClient.string(DHStrAllowClientInvalid))
        }
        if devices.isEmpty {
            iosNote(DeskhubClient.string(DHStrPairedEmpty))
        }
        ForEach(devices) { device in
            HStack(spacing: 12) {
                VStack(alignment: .leading, spacing: 2) {
                    Text(device.name.isEmpty ? DeskhubClient.string(DHStrUnnamedClient) : device.name)
                        .foregroundStyle(DeskhubPalette.heading)
                    iosHint(device.shortKey)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                Button(DeskhubClient.string(DHStrPairedForget)) {
                    forget(device)
                }
                .buttonStyle(.iosText())
            }
        }
        Button(DeskhubClient.string(DHStrPairedForgetAll)) {
            confirmForgetAll = true
        }
        .buttonStyle(.iosText())
        .disabled(devices.isEmpty)
        iosHint(DeskhubClient.string(DHStrPairedForgetNote))
    }

    @ViewBuilder private var savedHostsSection: some View {
        iosSection(DeskhubClient.string(DHStrSavedHostsHeading))
        iosNote(DeskhubClient.string(DHStrSavedHostsHint))
        if !savedHosts.listError.isEmpty {
            iosError(savedHosts.listError)
        } else if savedHosts.hosts.isEmpty {
            iosNote(DeskhubClient.string(DHStrSavedHostsEmpty))
        } else {
            ForEach(savedHosts.hosts) { host in
                savedHostRow(host)
            }
        }
        if !removeHostError.isEmpty {
            iosError(removeHostError)
        }
    }

    private func savedHostRow(_ host: SavedHostRow) -> some View {
        HStack(spacing: 8) {
            VStack(alignment: .leading, spacing: 2) {
                Text(host.alias.isEmpty ? host.endpoint : host.alias)
                    .foregroundStyle(DeskhubPalette.heading)
                iosHint("\(DeskhubClient.string(DHStrHostLastAddressLabel)): \(host.endpoint)")
                Text(host.fingerprint)
                    .font(.system(size: iosLabelSize))
                    .foregroundStyle(DeskhubPalette.muted)
            }
            .frame(maxWidth: .infinity, alignment: .leading)
            Button(DeskhubClient.string(DHStrConnectButton)) {
                onConnectHost(host.endpoint)
            }
            .buttonStyle(.iosText())
            Button(DeskhubClient.string(DHStrRemoveHostAction)) {
                removeHostError = savedHosts.removeReportingFailure(host)
            }
            .buttonStyle(.iosText())
        }
    }

    private func allowClient() {
        let key = trimmedPublicKey
        guard !key.isEmpty else { return }
        guard dh_paired_add_public_key(key) else {
            publicKeyInvalid = true
            return
        }
        publicKeyInput = ""
        publicKeyInvalid = false
        refreshPaired()
    }

    private func forget(_ device: PairedDeviceRow) {
        _ = dh_paired_forget(device.fingerprint)
        refreshPaired()
    }

    @MainActor
    private func pollAccessRequests() async {
        while !Task.isCancelled {
            accessRequests.refresh()
            try? await Task.sleep(for: IosDevicesPage.accessRequestsPollInterval)
        }
    }

    private func refresh() {
        deviceName = DeviceNameModel.sessionName
        hostFingerprint = DeskhubClient.buffered(IosDevicesPage.hostFingerprintCapacity) {
            dh_host_fingerprint($0, $1)
        }
        refreshPaired()
        savedHosts.refresh()
    }

    private func refreshPaired() {
        devices = DeskhubClient.ffiList(
            IosDevicesPage.pairedCapacity, DHPairedDevice(),
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
