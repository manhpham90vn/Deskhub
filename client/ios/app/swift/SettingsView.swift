import SwiftUI
import UIKit

struct IosLabeledField<Field: View>: View {
    private static var labelInset: CGFloat { 12 }
    private static var labelLift: CGFloat { -8 }

    let label: String
    let supportingText: String
    @ViewBuilder let field: Field

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            ZStack(alignment: .topLeading) {
                field.iosFieldBorder()
                Text(label)
                    .font(.system(size: iosHintSize))
                    .foregroundStyle(DeskhubPalette.muted)
                    .padding(.horizontal, 4)
                    .background(DeskhubPalette.page)
                    .offset(x: IosLabeledField.labelInset, y: IosLabeledField.labelLift)
            }
            iosHint(supportingText)
                .padding(.horizontal, IosLabeledField.labelInset)
        }
    }
}

struct SettingsView: View {
    private static let portSettle = Duration.milliseconds(600)

    @Bindable var settings: SettingsModel
    @State private var typedPort: String
    @State private var deviceName = DeviceNameModel()
    @FocusState private var editingDeviceName: Bool

    init(settings: SettingsModel) {
        self.settings = settings
        _typedPort = State(initialValue: String(settings.port))
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: iosPageSpacing) {
                iosHeading(DeskhubClient.string(DHStrClientSettingsHeading))
                iosNote(DeskhubClient.string(DHStrClientSettingsHint))

                deviceNameField

                iosSection(DeskhubClient.string(DHStrSettingsSectionConnection))
                portField

                iosSection(DeskhubClient.string(DHStrSettingsSectionSession))
                settingToggle(DHStrClipboardSyncLabel, isOn: $settings.clipboardSync)
                settingToggle(DHStrShareAudioLabel, isOn: $settings.shareAudio)
                settingToggle(DHStrPlayAudioLabel, isOn: $settings.playAudio)
                settingToggle(DHStrKeepAwakeLabel, isOn: $settings.keepAwake)

                ProjectFooter()
            }
            .padding(iosPagePadding)
        }
        .task(id: typedPort) {
            guard let chosen = Int(typedPort), SettingsModel.portRange.contains(chosen),
                  chosen != settings.port
            else { return }
            try? await Task.sleep(for: SettingsView.portSettle)
            guard !Task.isCancelled else { return }
            settings.updatePort(chosen)
        }
        .onChange(of: editingDeviceName) { _, editing in
            if !editing { deviceName.commit() }
        }
        .onDisappear { deviceName.commit() }
    }

    private var deviceNameField: some View {
        IosLabeledField(
            label: DeskhubClient.string(DHStrDeviceNameLabel),
            supportingText: DeskhubClient.string(DHStrDeviceNameHint)
        ) {
            TextField(UIDevice.current.model, text: $deviceName.name)
                .autocorrectionDisabled()
                .submitLabel(.done)
                .focused($editingDeviceName)
                .onSubmit(deviceName.commit)
        }
    }

    private var portField: some View {
        IosLabeledField(
            label: DeskhubClient.string(DHStrUdpPortLabel),
            supportingText: DeskhubClient.buffered(64) {
                dh_udp_port_line(UInt32(settings.port), $0, $1)
            }
        ) {
            TextField(String(settings.port), text: $typedPort)
                .keyboardType(.numberPad)
                .onChange(of: typedPort) { _, typed in
                    let limited = DigitsOnly.limit(typed, to: DigitsOnly.portLength)
                    if limited != typed { typedPort = limited }
                }
        }
    }

    private func settingToggle(_ label: DHStringId, isOn: Binding<Bool>) -> some View {
        Toggle(DeskhubClient.string(label), isOn: isOn)
            .tint(DeskhubPalette.accent)
            .onChange(of: isOn.wrappedValue) { _, _ in settings.save() }
    }
}

struct ProjectFooter: View {
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            if let url = URL(string: DeskhubClient.string(DHStrProjectUrl)) {
                Link(DeskhubClient.string(DHStrProjectLinkLabel), destination: url)
                    .font(.system(size: iosBodySize))
                    .foregroundStyle(DeskhubPalette.accent)
            }

            iosHint(DeskhubClient.buffered(64) { dh_version_line($0, $1) })
        }
        .padding(.top, 8)
    }
}
