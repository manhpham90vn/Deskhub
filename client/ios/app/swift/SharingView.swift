import SwiftUI
import UIKit

struct SharingView: View {
    @Bindable var model: SharingModel
    @State private var copiedIp: String?

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                deskhubHeading(DeskhubClient.string(DHStrSidebarHost))

                PasscodeCard(passcode: model.acceptedPasscode)

                HStack(spacing: 12) {
                    Text(DeskhubClient.string(DHStrBindInterfaceLabel))
                    Spacer(minLength: 0)
                    Picker("", selection: $model.bindIp) {
                        Text(DeskhubClient.string(DHStrBindAllInterfaces)).tag("")
                        ForEach(model.addresses) { address in
                            Text("\(address.ip)  (\(address.name))").tag(address.ip)
                        }
                        if staleBindIp {
                            Text(staleBindLabel).tag(model.bindIp)
                        }
                    }
                    .labelsHidden()
                    .disabled(model.status.sharing)
                }
                .onChange(of: model.bindIp) { _, _ in model.saveBindIp() }

                deskhubSection(DeskhubClient.string(DHStrHostIpIntro))
                if model.addresses.isEmpty {
                    deskhubHint(DeskhubClient.string(DHStrNoNetworkAddress))
                } else {
                    ForEach(shownAddresses) { address in
                        HStack(spacing: 12) {
                            Text(address.name).foregroundStyle(DeskhubPalette.muted)
                            Spacer(minLength: 0)
                            Text(address.ip).fontWeight(.bold).textSelection(.enabled)
                            Button {
                                UIPasteboard.general.string = address.ip
                                copiedIp = address.ip
                            } label: {
                                Image(
                                    systemName: copiedIp == address.ip
                                        ? "checkmark" : "doc.on.doc"
                                )
                            }
                            .buttonStyle(.borderless)
                            .accessibilityLabel("Copy address")
                        }
                    }
                }

                deskhubHint(DeskhubClient.string(DHStrSharingConnectHint))

                let receiving = FilesHost.shared.receiving
                deskhubSection(DeskhubClient.string(DHStrHostHeading))
                Text(
                    DeskhubClient.string(
                        model.status.sharing ? DHStrShareStateOn : DHStrShareStateOff
                    )
                )
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(
                    model.status.sharing ? DeskhubPalette.online : DeskhubPalette.muted
                )

                BroadcastPickerButton(
                    extensionBundleId: SharingModel.extensionBundleId,
                    title: DeskhubClient.string(
                        model.status.sharing ? DHStrStopSharing : DHStrStartSharing
                    )
                )

                deskhubHint(model.screenStatusLine)
                if model.status.sharing, model.status.memoryMB > 0 {
                    deskhubHint(
                        "\(DeskhubClient.string(DHStrBroadcastMemoryLabel)): "
                            + "\(model.status.memoryMB) MB"
                    )
                }
                if !model.status.error.isEmpty {
                    Text(model.status.error).foregroundStyle(DeskhubPalette.offline)
                }
                if model.status.sharing {
                    deskhubHint(viewerLine)
                }

                Divider()

                deskhubSection(DeskhubClient.string(DHStrFilesPickerLabel))
                if receiving {
                    Text(DeskhubClient.string(DHStrReceivingFilesState))
                        .font(.system(size: 15, weight: .semibold))
                        .foregroundStyle(DeskhubPalette.online)
                    if !model.filesStatusLine.isEmpty {
                        deskhubHint(model.filesStatusLine)
                    }
                }
                deskhubHint(DeskhubClient.string(DHStrMobileTakesFilesNote))
            }
            .padding()
        }
        .task { await model.poll() }
        .task(id: copiedIp) {
            guard copiedIp != nil else { return }
            try? await Task.sleep(for: .seconds(1.5))
            copiedIp = nil
        }
    }

    private var shownAddresses: [LocalAddress] {
        model.addresses.filter { model.bindIp.isEmpty || $0.ip == model.bindIp }
    }

    private var staleBindIp: Bool {
        !model.bindIp.isEmpty && !model.addresses.contains { $0.ip == model.bindIp }
    }

    private var staleBindLabel: String {
        "\(model.bindIp)  (\(DeskhubClient.string(DHStrBindNotConnectedNote)))"
    }

    private var viewerLine: String {
        guard model.status.sharing else { return DeskhubClient.string(DHStrNotSharing) }
        guard model.status.viewers > 0 else {
            return DeskhubClient.string(DHStrNothingShared)
        }
        guard !model.status.viewerNames.isEmpty else { return "\(model.status.viewers)" }
        return "\(model.status.viewers): \(model.status.viewerNames)"
    }
}
