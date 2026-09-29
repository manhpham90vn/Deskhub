import SwiftUI

struct PairedDeviceRow: Identifiable {
    let name: String
    let shortKey: String
    let fingerprint: String

    var id: String { fingerprint }
}

struct DevicesPage: View {
    private static let hostFingerprintCapacity = 128

    let trustedHostsRevision: Int
    let onConnectHost: @MainActor (String) -> Void
    @State private var devices: [PairedDeviceRow] = []
    @State private var keys = ClientKeysModel()
    @State private var hostFingerprint = ""
    @State private var confirmForgetAll = false
    @State private var publicKeyInput = ""
    @State private var publicKeyError = false

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            deskhubHeading(DeskhubClient.string(DHStrSidebarDevices))

            DeskhubArea(title: DeskhubClient.string(DHStrDevicesHostArea)) {
                deskhubHint(DeskhubClient.string(DHStrDevicesHostAreaHint))
                thisMachineSection
                pairedSection
            }

            DeskhubArea(title: DeskhubClient.string(DHStrDevicesClientArea)) {
                deskhubHint(DeskhubClient.string(DHStrDevicesClientAreaHint))
                MyKeysSection(model: keys)
                SavedHostsSection(
                    trustedHostsRevision: trustedHostsRevision,
                    onConnect: onConnectHost
                )
            }
        }
        .onAppear(perform: refresh)
        .alert("Deskhub", isPresented: $confirmForgetAll) {
            Button(DeskhubClient.string(DHStrPairedForgetAll), role: .destructive) {
                dh_paired_forget_all()
                refresh()
            }
            Button(DeskhubClient.string(DHStrCancelAction), role: .cancel) {}
        } message: {
            Text(DeskhubClient.string(DHStrPairedForgetAllPrompt))
        }
    }

    private var thisMachineSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubSection(DeskhubClient.string(DHStrThisMachineHeading))
            HStack(spacing: 12) {
                Text(hostFingerprint)
                    .font(.system(size: 13, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.heading)
                    .textSelection(.enabled)
                Spacer(minLength: 0)
                CopyButton { hostFingerprint }
                    .buttonStyle(.bordered)
                    .controlSize(.small)
            }
            deskhubHint(DeskhubClient.string(DHStrThisMachineHint))
        }
    }

    private var pairedSection: some View {
        VStack(alignment: .leading, spacing: 10) {
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
                    .disabled(publicKeyInput.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty)
            }
            if publicKeyError {
                Text(DeskhubClient.string(DHStrAllowClientInvalid))
                    .foregroundStyle(DeskhubPalette.offline)
            }
        }
    }

    @ViewBuilder
    private var pairedList: some View {
        #if os(macOS)
            Table(devices) {
                TableColumn(DeskhubClient.string(DHStrPairedColumnName)) { device in
                    Text(device.name.isEmpty ? "(unnamed)" : device.name)
                }
                .width(200)
                TableColumn(DeskhubClient.string(DHStrPairedColumnKey)) {
                    Text($0.shortKey)
                }
                .width(130)
                TableColumn("") { device in
                    Button(DeskhubClient.string(DHStrPairedForget)) {
                        forget(device)
                    }
                    .buttonStyle(.borderedProminent)
                    .controlSize(.small)
                    .tint(DeskhubPalette.offline)
                }
                .width(90)
            }
            .frame(minHeight: 130)

            if devices.isEmpty {
                deskhubHint(DeskhubClient.string(DHStrPairedEmpty))
            }

            Button {
                confirmForgetAll = true
            } label: {
                Text(DeskhubClient.string(DHStrPairedForgetAll))
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.bordered)
            .controlSize(.large)
            .disabled(devices.isEmpty)
        #else
            if devices.isEmpty {
                deskhubHint(DeskhubClient.string(DHStrPairedEmpty))
            } else {
                ForEach(devices) { device in
                    HStack(spacing: 12) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(device.name.isEmpty ? "(unnamed)" : device.name)
                                .foregroundStyle(DeskhubPalette.heading)
                            Text(device.shortKey)
                                .font(.caption)
                                .foregroundStyle(DeskhubPalette.muted)
                        }
                        Spacer(minLength: 0)
                        Button(DeskhubClient.string(DHStrPairedForget)) {
                            forget(device)
                        }
                        .controlSize(.small)
                    }
                    .padding(.vertical, 2)
                }
            }

            Button(DeskhubClient.string(DHStrPairedForgetAll)) {
                confirmForgetAll = true
            }
            .disabled(devices.isEmpty)
        #endif
    }

    private func allowClient() {
        guard dh_paired_add_public_key(publicKeyInput) else {
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

    private func refresh() {
        hostFingerprint = DeskhubClient.buffered(Self.hostFingerprintCapacity) {
            dh_host_fingerprint($0, $1)
        }
        keys.refresh()
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
