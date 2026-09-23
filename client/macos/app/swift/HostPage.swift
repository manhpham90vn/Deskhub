import SwiftUI

struct HostPage: View {
    @Bindable var sharing: SharingModel
    let onShare: () -> Void
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubHeading(DeskhubClient.string(DHStrHostHeading))
            deskhubHint(DeskhubClient.string(DHStrHostIpIntro))

            HStack(spacing: 14) {
                Text(DeskhubClient.string(DHStrBindInterfaceLabel))
                Picker("", selection: $sharing.bindIp) {
                    Text(DeskhubClient.string(DHStrBindAllInterfaces)).tag("")
                    ForEach(sharing.addresses) { address in
                        Text("\(address.ip)  (\(address.name))").tag(address.ip)
                    }
                    if staleBindIp {
                        Text(staleBindLabel).tag(sharing.bindIp)
                    }
                }
                .labelsHidden()
                .frame(width: 260)
                .disabled(sharing.isSharing || sharing.isStarting)
            }

            HostAddressList(
                addresses: shownAddresses,
                staleIp: staleBindIp ? sharing.bindIp : nil
            )

            if shareState != .idle {
                HostStatusBanner(state: shareState, detail: sharing.statusLine)
            }

            if sharing.isSharing || sharing.isStarting {
                PasscodeCard(passcode: sharing.acceptedPasscode, port: sharing.port)
            }

            if sharing.isSharing {
                HostSourceTable(
                    rows: sharing.rows,
                    onAction: { sharing.runRowAction($0) },
                    onAttach: { attachShell($0) },
                    onOpenFolder: { sharing.openFilesFolder($0) }
                )
                .frame(minHeight: 170)
            } else {
                SharePickerTable(
                    sources: sharing.shareSources,
                    ticked: $sharing.tickedSources,
                    terminal: $sharing.shareTerminal,
                    files: $sharing.shareFiles
                )
                .frame(minHeight: 170)
                deskhubHint(DeskhubClient.string(DHStrPickSourcesHint))
            }

            Button {
                onShare()
            } label: {
                HStack(spacing: 8) {
                    if sharing.isStarting {
                        ProgressView().controlSize(.small)
                    }
                    Text(shareState.action)
                }
                .deskhubPrimaryLabel()
            }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .tint(sharing.isSharing ? DeskhubPalette.offline : DeskhubPalette.accent)
            .disabled(sharing.isStarting)
        }
        .task { await sharing.refreshShareSources() }
        .onAppear { sharing.loadAddresses() }
        .onChange(of: sharing.bindIp) { _, _ in sharing.save() }
    }

    private func attachShell(_ row: HostRow) {
        guard sharing.stopAndAttachShell(row) else { return }
        openWindow(id: "localShell", value: row.termId)
    }

    private var shareState: HostShareState {
        if sharing.isStarting { return .starting }
        return sharing.isSharing ? .sharing : .idle
    }

    private var shownAddresses: [LocalAddress] {
        sharing.addresses.filter { sharing.bindIp.isEmpty || $0.ip == sharing.bindIp }
    }

    private var staleBindIp: Bool {
        !sharing.bindIp.isEmpty && !sharing.addresses.contains { $0.ip == sharing.bindIp }
    }

    private var staleBindLabel: String {
        "\(sharing.bindIp)  (\(DeskhubClient.string(DHStrBindNotConnectedNote)))"
    }
}

enum HostShareState: Equatable {
    case idle
    case starting
    case sharing

    var label: String {
        switch self {
        case .idle: DeskhubClient.string(DHStrShareStateOff)
        case .starting: DeskhubClient.string(DHStrStartingShare)
        case .sharing: DeskhubClient.string(DHStrShareStateOn)
        }
    }

    var action: String {
        switch self {
        case .idle, .starting: DeskhubClient.string(DHStrStartSharing)
        case .sharing: DeskhubClient.string(DHStrStopSharing)
        }
    }

    var tint: Color {
        switch self {
        case .idle: DeskhubPalette.muted
        case .starting: DeskhubPalette.accent
        case .sharing: DeskhubPalette.online
        }
    }
}

struct HostStatusBanner: View {
    let state: HostShareState
    let detail: String

    var body: some View {
        HStack(alignment: .top, spacing: 10) {
            VStack(alignment: .leading, spacing: 3) {
                Text(state.label)
                    .font(.system(size: 15, weight: .bold))
                    .foregroundStyle(state.tint)
                Text(detail)
                    .foregroundStyle(DeskhubPalette.muted)
                    .fixedSize(horizontal: false, vertical: true)
            }

            Spacer(minLength: 0)
        }
        .padding(12)
        .padding(.leading, 4)
        .background(
            RoundedRectangle(cornerRadius: 10).fill(state.tint.opacity(0.12))
        )
        .overlay(alignment: .leading) {
            Rectangle().fill(state.tint).frame(width: 4)
        }
        .clipShape(RoundedRectangle(cornerRadius: 10))
        .overlay(
            RoundedRectangle(cornerRadius: 10).stroke(state.tint.opacity(0.35), lineWidth: 1)
        )
    }
}

struct HostSourceTable: View {
    let rows: [HostRow]
    let onAction: (HostRow) -> Void
    let onAttach: (HostRow) -> Void
    let onOpenFolder: (HostRow) -> Void

    var body: some View {
        Table(rows) {
            TableColumn("Source") { cell($0, $0.source) }.width(min: 96, ideal: 120)
            TableColumn("Size") { cell($0, $0.size) }.width(min: 60, ideal: 70)
            TableColumn("Viewers") { cell($0, $0.viewers) }.width(min: 44, ideal: 50)
            TableColumn("Client") { cell($0, $0.client) }.width(min: 80, ideal: 100)
            TableColumn("Capture") { cell($0, $0.capture) }.width(min: 46, ideal: 50)
            TableColumn("Send") { cell($0, $0.send) }.width(min: 42, ideal: 44)
            TableColumn("Mbps") { cell($0, $0.mbps) }.width(min: 44, ideal: 48)
            TableColumn("RTT") { cell($0, $0.rtt) }.width(min: 42, ideal: 48)
            TableColumn("") { action($0) }.width(92)
            TableColumn("") { attach($0) }.width(110)
        }
    }

    private func cell(_ row: HostRow, _ text: String) -> some View {
        Text(text).foregroundStyle(row.online ? DeskhubPalette.online : DeskhubPalette.heading)
    }

    private func action(_ row: HostRow) -> some View {
        let remoteRow = row.viewer && !row.attachedLocally
        return Button(
            DeskhubClient.string(
                remoteRow ? DHStrDisconnectViewerAction : DHStrStopDisplayAction
            )
        ) {
            onAction(row)
        }
        .buttonStyle(.borderedProminent)
        .controlSize(.small)
        .tint(remoteRow ? DeskhubPalette.warning : DeskhubPalette.offline)
    }

    @ViewBuilder
    private func attach(_ row: HostRow) -> some View {
        if row.canAttachLocally {
            Button(DeskhubClient.string(DHStrAttachShellAction)) { onAttach(row) }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .tint(DeskhubPalette.offline)
        } else if row.files, !row.viewer {
            Button(DeskhubClient.string(DHStrOpenFolderAction)) { onOpenFolder(row) }
                .buttonStyle(.borderedProminent)
                .controlSize(.small)
                .tint(DeskhubPalette.accent)
        }
    }
}

struct SharePickerTable: View {
    let sources: [ShareSource]
    @Binding var ticked: Set<UInt32>
    @Binding var terminal: Bool
    @Binding var files: Bool

    private enum Kind {
        case display(UInt32)
        case terminal
        case files
    }

    private struct Row: Identifiable {
        let id: String
        let name: String
        let size: String
        let kind: Kind
    }

    private var rows: [Row] {
        var all = sources.map { source in
            Row(id: "source-\(source.id)", name: source.name,
                size: "\(source.width)x\(source.height)", kind: .display(source.id))
        }
        all.append(Row(id: "terminal",
                       name: DeskhubClient.string(DHStrTerminalPickerLabel),
                       size: "", kind: .terminal))
        all.append(Row(id: "files",
                       name: DeskhubClient.string(DHStrFilesPickerLabel),
                       size: "", kind: .files))
        return all
    }

    var body: some View {
        Table(rows) {
            TableColumn("") { row in
                Toggle("", isOn: tick(row)).labelsHidden()
            }
            .width(24)
            TableColumn("Source") { Text($0.name) }.width(220)
            TableColumn("Size") { Text($0.size) }.width(110)
        }
    }

    private func tick(_ row: Row) -> Binding<Bool> {
        switch row.kind {
        case .terminal:
            $terminal
        case .files:
            $files
        case let .display(sourceId):
            Binding(
                get: { ticked.contains(sourceId) },
                set: { on in
                    if on {
                        ticked.insert(sourceId)
                    } else {
                        ticked.remove(sourceId)
                    }
                }
            )
        }
    }
}
