import SwiftUI

struct PairedDeviceRow: Identifiable {
    let name: String
    let shortKey: String
    let fingerprint: String
    let pairedUnix: Int64
    let lastSeenUnix: Int64

    var id: String { fingerprint }
}

struct DevicesPage: View {
    @State private var devices: [PairedDeviceRow] = []
    @State private var allowPairing = dh_allow_pairing()
    @State private var confirmForgetAll = false
    @State private var publicKeyInput = ""
    @State private var publicKeyError = false

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubHeading(DeskhubClient.string(DHStrPairedHeading))
            deskhubHint(DeskhubClient.string(DHStrPairedHint))

            TextField("Public key to allow", text: $publicKeyInput)
            Button("Add public key") {
                if dh_paired_add_public_key(publicKeyInput) {
                    publicKeyInput = ""
                    publicKeyError = false
                    refresh()
                } else {
                    publicKeyError = true
                }
            }
            .disabled(publicKeyInput.isEmpty)
            if publicKeyError {
                Text("Invalid public key or it could not be saved.")
                    .foregroundStyle(DeskhubPalette.offline)
            }

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
                    TableColumn(DeskhubClient.string(DHStrPairedColumnPaired)) {
                        Text(Self.dateText($0.pairedUnix))
                    }
                    .width(150)
                    TableColumn(DeskhubClient.string(DHStrPairedColumnLastSeen)) {
                        Text(Self.dateText($0.lastSeenUnix))
                    }
                    .width(150)
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
                                Text(Self.subtitle(for: device))
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

            deskhubHint(DeskhubClient.string(DHStrPairedForgetNote))

            Toggle(DeskhubClient.string(DHStrAllowPairingLabel), isOn: $allowPairing)
                .onChange(of: allowPairing) { _, allow in dh_set_allow_pairing(allow) }
            deskhubHint(DeskhubClient.string(DHStrAllowPairingHint))

            deskhubSection(DeskhubClient.string(DHStrThisMachineHeading))
            Text(DeskhubClient.buffered(1024) { dh_own_public_key($0, $1) })
                .font(.system(size: 12, design: .monospaced))
                .textSelection(.enabled)
            Text(DeskhubClient.buffered(128) { dh_own_fingerprint($0, $1) })
                .font(.system(size: 13, design: .monospaced))
                .foregroundStyle(DeskhubPalette.heading)
                .textSelection(.enabled)
            deskhubHint(DeskhubClient.string(DHStrThisMachineHint))
        }
        .onAppear(perform: refresh)
        .alert("Deskhub", isPresented: $confirmForgetAll) {
            Button(DeskhubClient.string(DHStrPairedForgetAll), role: .destructive) {
                dh_paired_forget_all()
                refresh()
            }
            Button("Cancel", role: .cancel) {}
        } message: {
            Text(DeskhubClient.string(DHStrPairedForgetAllPrompt))
        }
    }

    private func forget(_ device: PairedDeviceRow) {
        _ = dh_paired_forget(device.fingerprint)
        refresh()
    }

    private func refresh() {
        devices = DeskhubClient.ffiList(
            128, DHPairedDevice(),
            { dh_paired_devices($0, $1) },
            { raw in
                PairedDeviceRow(
                    name: DeskhubClient.cString(raw.name),
                    shortKey: DeskhubClient.cString(raw.shortKey),
                    fingerprint: DeskhubClient.cString(raw.fingerprint),
                    pairedUnix: raw.pairedUnix,
                    lastSeenUnix: raw.lastSeenUnix
                )
            }
        )
        allowPairing = dh_allow_pairing()
    }

    private static func subtitle(for device: PairedDeviceRow) -> String {
        var line = device.shortKey
        line += "  ·  "
        line += DeskhubClient.string(DHStrPairedColumnPaired)
        line += " "
        line += Self.dateText(device.pairedUnix)
        line += "  ·  "
        line += DeskhubClient.string(DHStrPairedColumnLastSeen)
        line += " "
        line += Self.dateText(device.lastSeenUnix)
        return line
    }

    private static func dateText(_ unix: Int64) -> String {
        guard unix > 0 else { return "-" }
        let date = Date(timeIntervalSince1970: TimeInterval(unix))
        return date.formatted(date: .numeric, time: .shortened)
    }
}
