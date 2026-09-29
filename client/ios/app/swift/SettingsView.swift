import SwiftUI

struct SettingsView: View {
    private static let portSettle = Duration.milliseconds(600)

    @Bindable var settings: SettingsModel
    let onPortChange: (UInt16) -> Void

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                deskhubHeading(DeskhubClient.string(DHStrClientSettingsHeading))
                deskhubHint(DeskhubClient.string(DHStrClientSettingsHint))

                deskhubSection(DeskhubClient.string(DHStrSettingsSectionConnection))
                HStack(spacing: 12) {
                    Text("UDP port")
                    Spacer(minLength: 0)
                    TextField("", value: $settings.port, format: .number.grouping(.never))
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.numberPad)
                        .multilineTextAlignment(.trailing)
                        .frame(width: 110)
                }
                deskhubHint(
                    DeskhubClient.buffered(64) {
                        dh_udp_port_line(UInt32(settings.acceptedPort), $0, $1)
                    }
                )

                deskhubSection(DeskhubClient.string(DHStrSettingsSectionSession))
                Toggle(isOn: $settings.clipboardSync) {
                    Text(DeskhubClient.string(DHStrClipboardSyncLabel))
                }
                .onChange(of: settings.clipboardSync) { _, _ in
                    settings.save()
                }
                Toggle(isOn: $settings.shareAudio) {
                    Text(DeskhubClient.string(DHStrShareAudioLabel))
                }
                .onChange(of: settings.shareAudio) { _, _ in
                    settings.save()
                }
                Toggle(isOn: $settings.playAudio) {
                    Text(DeskhubClient.string(DHStrPlayAudioLabel))
                }
                .onChange(of: settings.playAudio) { _, _ in
                    settings.save()
                }
                Toggle(isOn: $settings.keepAwake) {
                    Text(DeskhubClient.string(DHStrKeepAwakeLabel))
                }
                .onChange(of: settings.keepAwake) { _, _ in
                    settings.save()
                }

                ProjectFooter()
            }
            .padding()
        }
        .task(id: settings.port) {
            try? await Task.sleep(for: SettingsView.portSettle)
            guard !Task.isCancelled else { return }
            settings.save()
            onPortChange(settings.acceptedPort)
        }
    }
}

struct ProjectFooter: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if let url = URL(string: DeskhubClient.string(DHStrProjectUrl)) {
                Link(DeskhubClient.string(DHStrProjectLinkLabel), destination: url)
            }

            Text(DeskhubClient.buffered(64) { dh_version_line($0, $1) })
                .font(.caption)
                .foregroundStyle(DeskhubPalette.muted)
        }
        .padding(.top, 8)
    }
}
