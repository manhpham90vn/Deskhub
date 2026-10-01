import SwiftUI

struct SharingView: View {
    @Bindable var model: SharingModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: iosPageSpacing) {
                iosHeading(DeskhubClient.string(DHStrSidebarHost))
                networkRow
                addressSection
                iosHint(DeskhubClient.string(DHStrSharingConnectHint))
                if model.status.sharing {
                    qrSection
                }
                shareSection
                IosAccessRequestsSection(model: model.accessRequests)
                iosNote(model.hostRowsLine)
                Divider()
                filesSection
            }
            .padding(iosPagePadding)
        }
        .task { await model.poll() }
    }

    private var networkRow: some View {
        HStack(spacing: 8) {
            Text(DeskhubClient.string(DHStrBindInterfaceLabel))
                .frame(maxWidth: .infinity, alignment: .leading)
            Menu {
                Button(DeskhubClient.string(DHStrBindAllInterfaces)) { model.bindIp = "" }
                ForEach(model.addresses) { address in
                    Button("\(address.ip)  (\(address.name))") { model.bindIp = address.ip }
                }
            } label: {
                Text(model.bindLabel)
                    .font(.system(size: iosBodySize, weight: .semibold))
                    .foregroundStyle(
                        model.status.sharing ? DeskhubPalette.muted : DeskhubPalette.accent
                    )
                    .padding(.horizontal, 12)
                    .frame(minHeight: 40)
            }
            .disabled(model.status.sharing)
        }
        .onChange(of: model.bindIp) { _, _ in model.saveBindIp() }
    }

    @ViewBuilder private var addressSection: some View {
        iosHeading(DeskhubClient.string(DHStrHostIpIntro))
        if model.addresses.isEmpty {
            iosNote(DeskhubClient.string(DHStrNoNetworkAddress))
        } else {
            ForEach(model.shownAddresses) { address in
                HStack(spacing: 12) {
                    Text(address.name)
                        .foregroundStyle(DeskhubPalette.muted)
                        .frame(maxWidth: .infinity, alignment: .leading)
                    Text(address.ip)
                        .fontWeight(.bold)
                        .foregroundStyle(DeskhubPalette.heading)
                        .textSelection(.enabled)
                    Button(DeskhubClient.string(DHStrCopyButton)) {
                        DeskhubPasteboard.copy(address.ip)
                    }
                    .buttonStyle(.iosText())
                }
            }
        }
    }

    @ViewBuilder private var qrSection: some View {
        Button(DeskhubClient.string(model.qr.open ? DHStrHideQrAction : DHStrShowQrAction)) {
            model.toggleQr()
        }
        .buttonStyle(.iosOutlined(fullWidth: true))
        if model.qrUnavailable {
            iosError(DeskhubClient.string(DHStrQrUnavailable))
        }
        if model.qr.open {
            IosQrInvitePanel(sharing: model)
        }
    }

    @ViewBuilder private var shareSection: some View {
        iosSection(DeskhubClient.string(DHStrHostHeading))
        Text(DeskhubClient.string(model.status.sharing ? DHStrShareStateOn : DHStrShareStateOff))
            .font(.system(size: iosSectionSize, weight: .medium))
            .foregroundStyle(model.status.sharing ? DeskhubPalette.online : DeskhubPalette.muted)
        BroadcastPickerButton(
            extensionBundleId: SharingModel.extensionBundleId,
            title: DeskhubClient.string(
                model.status.sharing ? DHStrStopSharing : DHStrStartSharing
            )
        )
        iosNote(model.screenStatusLine)
        if model.status.sharing, model.status.memoryMB > 0 {
            iosNote(
                "\(DeskhubClient.string(DHStrBroadcastMemoryLabel)): \(model.status.memoryMB) MB"
            )
        }
        if !model.status.error.isEmpty {
            iosError(model.status.error)
        }
    }

    @ViewBuilder private var filesSection: some View {
        iosSection(DeskhubClient.string(DHStrFilesPickerLabel))
        if FilesHost.shared.receiving {
            Text(DeskhubClient.string(DHStrReceivingFilesState))
                .font(.system(size: iosSectionSize, weight: .medium))
                .foregroundStyle(DeskhubPalette.online)
            if !model.filesStatusLine.isEmpty {
                iosNote(model.filesStatusLine)
            }
        }
        iosHint(DeskhubClient.string(DHStrMobileTakesFilesNote))
    }
}
