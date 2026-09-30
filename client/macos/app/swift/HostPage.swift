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
                .disabled(live)
            }

            HStack(alignment: .top, spacing: 14) {
                VStack(alignment: .leading, spacing: 10) {
                    HostAddressList(
                        addresses: shownAddresses,
                        staleIp: staleBindIp ? sharing.bindIp : nil
                    )
                }
                if live {
                    Spacer(minLength: 0)
                    PairingQrToggleButton(
                        model: sharing.qr, port: sharing.sharingPort, bindIp: sharing.bindIp
                    )
                    .disabled(!sharing.isSharing)
                }
            }

            if live, sharing.qr.shown {
                PairingQrPanel(model: sharing.qr)
            }

            if shareState != .idle {
                HostStatusBanner(state: shareState, detail: sharing.statusLine)
            }

            if live {
                PortCard(port: sharing.shownPort)
                AccessRequestsSection(model: sharing.accessRequests)
                HostSourceTable(
                    rows: sharing.rows,
                    onAction: { sharing.runRowAction($0) },
                    onAttach: { attachShell($0) },
                    onOpenFolder: { sharing.openFilesFolder($0) }
                )
                .frame(minHeight: DeskhubPrimaryMetrics.pickerHeight)
            } else {
                SharePickerList(
                    sources: sharing.shareSources,
                    ticked: $sharing.tickedSources,
                    terminal: $sharing.shareTerminal,
                    files: $sharing.shareFiles
                )
                deskhubHint(DeskhubClient.string(DHStrPickSourcesHint))
            }

            Button(action: onShare) {
                Text(shareState.action)
            }
            .buttonStyle(DeskhubPrimaryButtonStyle(fill: shareState.buttonFill))
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

    private var live: Bool {
        sharing.isSharing || sharing.isStarting
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
        case .idle: DeskhubClient.string(DHStrStartSharing)
        case .starting, .sharing: DeskhubClient.string(DHStrStopSharing)
        }
    }

    var buttonFill: Color {
        switch self {
        case .idle, .starting: DeskhubPalette.accent
        case .sharing: DeskhubPalette.offline
        }
    }

    var textColor: Color {
        switch self {
        case .idle: DeskhubPalette.muted
        case .starting: DeskhubPalette.accentPressed
        case .sharing: DeskhubPalette.onlineText
        }
    }

    var barColor: Color {
        switch self {
        case .idle: DeskhubPalette.muted
        case .starting: DeskhubPalette.accent
        case .sharing: DeskhubPalette.online
        }
    }

    var background: Color {
        switch self {
        case .idle: DeskhubPalette.panelIdle
        case .starting: DeskhubPalette.panelBusy
        case .sharing: DeskhubPalette.panelLive
        }
    }
}

struct HostStatusBanner: View {
    let state: HostShareState
    let detail: String

    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(state.label)
                .font(.system(size: 15, weight: .bold))
                .foregroundStyle(state.textColor)
            Text(detail)
                .foregroundStyle(DeskhubPalette.muted)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(10)
        .padding(.leading, 4)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(state.background)
        .overlay(alignment: .leading) {
            Rectangle().fill(state.barColor).frame(width: 4)
        }
    }
}

struct HostSourceTable: View {
    let rows: [HostRow]
    let onAction: (HostRow) -> Void
    let onAttach: (HostRow) -> Void
    let onOpenFolder: (HostRow) -> Void

    private struct Column {
        let title: String
        let width: CGFloat
        let alignment: Alignment
        let mono: Bool
    }

    private static let metrics = dh_host_table_metrics()
    private static let cellGap = CGFloat(metrics.cellGap)
    private static let barWidth = CGFloat(metrics.barWidth)
    private static let rowHeight = CGFloat(metrics.rowHeight)
    private static let headerHeight = CGFloat(metrics.headerHeight)
    private static let ruleMargin = CGFloat(metrics.ruleMargin)

    private static let columns = DeskhubClient.ffiList(
        16, DHHostColumn(), { dh_host_columns($0, $1) },
        { raw in
            Column(
                title: DeskhubClient.cString(raw.title),
                width: CGFloat(raw.width),
                alignment: raw.trailing ? .trailing : .leading,
                mono: raw.mono
            )
        }
    )

    var body: some View {
        VStack(spacing: 0) {
            header
            Rectangle().fill(DeskhubPalette.rowLine).frame(height: 1)
            ScrollView([.vertical, .horizontal]) {
                VStack(alignment: .leading, spacing: 0) {
                    ForEach(Array(rows.enumerated()), id: \.element.id) { index, row in
                        if !row.viewer, index > 0 {
                            Rectangle().fill(DeskhubPalette.rowLine).frame(height: 1)
                                .padding(.vertical, HostSourceTable.ruleMargin)
                        }
                        rowView(row)
                    }
                }
            }
        }
        .overlay(Rectangle().stroke(DeskhubPalette.rowLine, lineWidth: 1))
    }

    private var header: some View {
        HStack(spacing: HostSourceTable.cellGap) {
            Color.clear.frame(width: HostSourceTable.barWidth)
            ForEach(HostSourceTable.columns, id: \.title) { column in
                Text(column.title)
                    .font(.system(size: 11, weight: .bold))
                    .foregroundStyle(DeskhubPalette.muted)
                    .frame(width: column.width, alignment: column.alignment)
            }
            Spacer(minLength: 0)
        }
        .frame(height: HostSourceTable.headerHeight)
        .background(DeskhubPalette.panelIdle)
    }

    private func rowView(_ row: HostRow) -> some View {
        let texts = [row.source, row.size, row.viewers, row.client, row.capture, row.send,
                     row.mbps, row.rtt]
        return HStack(spacing: HostSourceTable.cellGap) {
            Rectangle()
                .fill(row.online ? DeskhubPalette.online : DeskhubPalette.rowLine)
                .frame(width: HostSourceTable.barWidth)
            ForEach(Array(HostSourceTable.columns.enumerated()), id: \.offset) { index, column in
                cell(texts[index], column: column, bold: index == 0 && !row.viewer,
                     online: row.online)
            }
            action(row)
            attach(row)
            Spacer(minLength: 0)
        }
        .frame(height: HostSourceTable.rowHeight)
        .background(row.viewer ? DeskhubPalette.viewerRow : Color.clear)
    }

    private func cell(_ text: String, column: Column, bold: Bool, online: Bool) -> some View {
        Text(text)
            .font(column.mono ? .system(.body, design: .monospaced) : .body)
            .fontWeight(bold ? .bold : .regular)
            .foregroundStyle(online ? DeskhubPalette.heading : DeskhubPalette.muted)
            .lineLimit(1)
            .truncationMode(.tail)
            .frame(width: column.width, alignment: column.alignment)
    }

    @ViewBuilder
    private func action(_ row: HostRow) -> some View {
        if row.files, row.viewer {
            Color.clear.frame(width: DeskhubListMetrics.actionWidth)
        } else {
            let remoteRow = row.viewer && !row.attachedLocally
            let title = remoteRow ? DHStrDisconnectViewerAction : DHStrStopDisplayAction
            DeskhubRowActionButton(
                title: DeskhubClient.string(title),
                tint: remoteRow ? DeskhubPalette.warning : DeskhubPalette.offline
            ) { onAction(row) }
        }
    }

    @ViewBuilder
    private func attach(_ row: HostRow) -> some View {
        if row.canAttachLocally {
            DeskhubRowActionButton(
                title: DeskhubClient.string(DHStrAttachShellAction), tint: DeskhubPalette.offline
            ) { onAttach(row) }
        } else if row.files, !row.viewer {
            DeskhubRowActionButton(
                title: DeskhubClient.string(DHStrOpenFolderAction), tint: DeskhubPalette.accent
            ) { onOpenFolder(row) }
        }
    }
}

extension ShareSource {
    func pickerLabel(index: Int) -> String {
        let shownName = name.isEmpty ? "Source \(index)" : name
        return "\(shownName) (\(width)x\(height))"
    }
}

struct SharePickerList: View {
    let sources: [ShareSource]
    @Binding var ticked: Set<UInt32>
    @Binding var terminal: Bool
    @Binding var files: Bool

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            ForEach(Array(sources.enumerated()), id: \.element.id) { index, source in
                Toggle(source.pickerLabel(index: index), isOn: tick(source.id))
                    .toggleStyle(.checkbox)
            }
            Toggle(DeskhubClient.string(DHStrTerminalPickerLabel), isOn: $terminal)
                .toggleStyle(.checkbox)
            Toggle(DeskhubClient.string(DHStrFilesPickerLabel), isOn: $files)
                .toggleStyle(.checkbox)
        }
        .padding(8)
        .deskhubBorderedBox(minHeight: DeskhubPrimaryMetrics.pickerHeight)
    }

    private func tick(_ sourceId: UInt32) -> Binding<Bool> {
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
