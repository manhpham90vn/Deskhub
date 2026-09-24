import SwiftUI

struct SettingsPage: View {
    @Bindable var sharing: SharingModel

    private static let areas = SettingsAreaModel.loadDesktopLayout()

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            deskhubHeading(DeskhubClient.string(DHStrSidebarSettings))

            ForEach(SettingsPage.areas) { area in
                SettingsArea(title: area.title) {
                    ForEach(area.blocks) { block in
                        blockView(block)
                    }
                }
            }
        }
        .onChange(of: sharing.fps) { _, _ in sharing.save() }
        .onChange(of: sharing.bitrateMbps) { _, _ in sharing.save() }
        .onChange(of: sharing.maxDim) { _, _ in sharing.save() }
        .onChange(of: sharing.port) { _, _ in sharing.save() }
        .onChange(of: sharing.passcode) { _, _ in sharing.save() }
        .onChange(of: sharing.allowInput) { _, _ in sharing.save() }
        .onChange(of: sharing.autoShare) { _, _ in sharing.save() }
        .onChange(of: sharing.autostart) { _, _ in sharing.applyAutostart() }
        .onChange(of: sharing.startHidden) { _, _ in sharing.save() }
        .onChange(of: sharing.clipboardSync) { _, _ in sharing.save() }
        .onChange(of: sharing.shareAudio) { _, _ in sharing.save() }
        .onChange(of: sharing.playAudio) { _, _ in sharing.save() }
        .onChange(of: sharing.keepAwake) { _, _ in sharing.save() }
    }

    @ViewBuilder
    private func blockView(_ block: SettingsBlock) -> some View {
        switch block.content {
        case let .hint(text):
            deskhubHint(text)
        case let .section(title):
            deskhubSection(title)
        case let .inputs(rows):
            Grid(alignment: .leading, horizontalSpacing: 14, verticalSpacing: 10) {
                ForEach(rows) { row in
                    GridRow {
                        Text(row.label)
                        input(row.field)
                    }
                }
            }
        case let .setting(row):
            setting(row)
        }
    }

    @ViewBuilder
    private func input(_ field: DHSettingField) -> some View {
        switch field {
        case DHSettingFps:
            TextField("", value: $sharing.fps, format: .number)
                .textFieldStyle(.roundedBorder).frame(width: 90)
        case DHSettingBitrate:
            TextField("", value: $sharing.bitrateMbps, format: .number)
                .textFieldStyle(.roundedBorder).frame(width: 90)
        case DHSettingQuality:
            Picker("", selection: $sharing.maxDim) {
                ForEach(DeskhubShare.qualityPresets) { preset in
                    Text(preset.label).tag(preset.maxDim)
                }
            }
            .labelsHidden()
            .frame(width: 120)
        case DHSettingPort:
            TextField("", value: $sharing.port, format: .number.grouping(.never))
                .textFieldStyle(.roundedBorder).frame(width: 90)
        case DHSettingPasscode:
            PasscodeField(passcode: $sharing.passcode, width: 64)
        default:
            EmptyView()
        }
    }

    @ViewBuilder
    private func setting(_ row: SettingRow) -> some View {
        switch row.field {
        case DHSettingTransferFolder:
            transferFolderRow(row.label)
        case DHSettingPermissions:
            PermissionsSection(sharing: sharing)
        default:
            if let isOn = flag(row.field) {
                Toggle(row.label, isOn: isOn).toggleStyle(.checkbox)
            }
        }
    }

    private func flag(_ field: DHSettingField) -> Binding<Bool>? {
        switch field {
        case DHSettingAllowInput: $sharing.allowInput
        case DHSettingShareAudio: $sharing.shareAudio
        case DHSettingAutoShare: $sharing.autoShare
        case DHSettingPlayAudio: $sharing.playAudio
        case DHSettingClipboardSync: $sharing.clipboardSync
        case DHSettingKeepAwake: $sharing.keepAwake
        case DHSettingAutostart: $sharing.autostart
        case DHSettingCloseToTray: $sharing.startHidden
        default: nil
        }
    }

    private func transferFolderRow(_ label: String) -> some View {
        HStack(spacing: 8) {
            Text(label)
            Text(sharing.transferFolder)
                .foregroundStyle(DeskhubPalette.muted)
                .lineLimit(1)
                .truncationMode(.middle)
                .frame(maxWidth: .infinity, alignment: .leading)
            Button(DeskhubClient.string(DHStrTransferChooseButton)) {
                sharing.chooseTransferFolder()
            }
        }
    }
}

private struct SettingRow: Identifiable {
    let id: Int
    let field: DHSettingField
    let label: String
}

private struct SettingsBlock: Identifiable {
    enum Content {
        case hint(String)
        case section(String)
        case inputs([SettingRow])
        case setting(SettingRow)
    }

    let id: Int
    var content: Content
}

private struct SettingsAreaModel: Identifiable {
    private static let layoutCapacity = 64
    private static let labelledInputs: [DHSettingField] = [
        DHSettingFps, DHSettingBitrate, DHSettingQuality, DHSettingPort, DHSettingPasscode,
    ]

    let id: Int
    let title: String
    var blocks: [SettingsBlock] = []

    static func loadDesktopLayout() -> [SettingsAreaModel] {
        let entries = DeskhubClient.ffiList(
            layoutCapacity, DHSettingsEntry(), { dh_settings_layout($0, $1) },
            { raw in (kind: raw.kind, field: raw.field, text: DeskhubClient.cString(raw.text)) }
        )
        var areas: [SettingsAreaModel] = []
        for (index, entry) in entries.enumerated() {
            if entry.kind == DHSettingsEntryArea {
                areas.append(SettingsAreaModel(id: index, title: entry.text))
                continue
            }
            guard !areas.isEmpty else { continue }
            areas[areas.count - 1].add(index: index, kind: entry.kind, field: entry.field,
                                       text: entry.text)
        }
        return areas
    }

    private mutating func add(index: Int, kind: DHSettingsEntryKind, field: DHSettingField,
                              text: String)
    {
        switch kind {
        case DHSettingsEntryHint:
            blocks.append(SettingsBlock(id: index, content: .hint(text)))
        case DHSettingsEntrySection:
            blocks.append(SettingsBlock(id: index, content: .section(text)))
        default:
            let row = SettingRow(id: index, field: field, label: text)
            guard SettingsAreaModel.labelledInputs.contains(field) else {
                blocks.append(SettingsBlock(id: index, content: .setting(row)))
                return
            }
            if let last = blocks.last, case let .inputs(rows) = last.content {
                blocks[blocks.count - 1].content = .inputs(rows + [row])
            } else {
                blocks.append(SettingsBlock(id: index, content: .inputs([row])))
            }
        }
    }
}

private struct SettingsArea<Content: View>: View {
    private static var accentBarWidth: CGFloat { CGFloat(dh_settings_area_bar_width()) }

    let title: String
    @ViewBuilder let content: Content

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.system(size: 17, weight: .bold))
                .foregroundStyle(DeskhubPalette.heading)
            content
        }
        .padding(16)
        .padding(.leading, SettingsArea.accentBarWidth)
        .frame(maxWidth: .infinity, alignment: .leading)
        .overlay(alignment: .leading) {
            Rectangle().fill(DeskhubPalette.accent).frame(width: SettingsArea.accentBarWidth)
        }
        .clipShape(RoundedRectangle(cornerRadius: 8))
        .overlay(
            RoundedRectangle(cornerRadius: 8).stroke(DeskhubPalette.rowLine, lineWidth: 1)
        )
    }
}
