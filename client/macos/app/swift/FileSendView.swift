import AppKit
import SwiftUI

struct FileSendView<Driver: TransferDriver>: View {
    let model: Driver

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(DeskhubClient.string(DHStrTransferSendHeading))
                .font(.headline)

            chosenList

            if !model.transferError.isEmpty {
                Text(model.transferError)
                    .font(.callout)
                    .foregroundStyle(DeskhubPalette.offline)
            }

            if !model.transfer.idle {
                progress
            }

            sent

            HStack {
                Button(DeskhubClient.string(DHStrTransferChooseButton)) { choose() }
                    .disabled(model.transfer.active)
                Spacer()
                if model.transfer.active {
                    Button(DeskhubClient.string(DHStrTransferCancelButton)) {
                        model.cancelTransfer()
                    }
                }
                Button("Send") { model.sendChosenFiles() }
                    .keyboardShortcut(.defaultAction)
                    .disabled(!model.canSend)
            }
        }
        .padding(20)
        .frame(minWidth: 460, maxWidth: .infinity, alignment: .topLeading)
        .trustPrompt(
            isPresented: Binding(
                get: { !model.changedKeyFingerprint.isEmpty },
                set: { _ in }
            ),
            changed: true,
            fingerprint: model.changedKeyFingerprint
        ) { model.answerChangedKey(accept: $0) }
    }

    @ViewBuilder
    private var chosenList: some View {
        if model.chosenFiles.isEmpty {
            Text(DeskhubClient.string(DHStrTransferNoneChosen))
                .foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, alignment: .leading)
        } else {
            List(model.chosenFiles, id: \.self) { url in
                HStack {
                    Text(url.lastPathComponent).lineLimit(1).truncationMode(.middle)
                    Spacer()
                    Text(sizeText(url)).foregroundStyle(.secondary).monospacedDigit()
                }
            }
            .frame(height: 140)
        }
    }

    private var progress: some View {
        VStack(alignment: .leading, spacing: 4) {
            ProgressView(value: model.transfer.fraction)
            HStack {
                Text(statusText).lineLimit(1).truncationMode(.middle)
                Spacer()
                Text(model.transfer.step).monospacedDigit().foregroundStyle(.secondary)
            }
            .font(.callout)
        }
    }

    private var statusText: String {
        guard model.transfer.active else { return model.transfer.message }
        if !model.transfer.name.isEmpty { return model.transfer.name }
        if !model.transfer.message.isEmpty { return model.transfer.message }
        return DeskhubClient.string(DHStrTransferSending)
    }

    @ViewBuilder private var sent: some View {
        if !model.history.isEmpty {
            Divider()
            Text(DeskhubClient.string(DHStrTransferSentHeading))
                .font(.subheadline)
                .foregroundStyle(.secondary)
            List(model.history) { row in
                HStack(spacing: 8) {
                    Image(systemName: row.ok ? "checkmark.circle" : "xmark.circle")
                        .foregroundStyle(row.ok ? DeskhubPalette.online : DeskhubPalette.offline)
                    Text(row.name).lineLimit(1).truncationMode(.middle)
                    Spacer()
                    Text(row.ok ? byteText(row.bytes) : row.detail)
                        .foregroundStyle(.secondary)
                        .lineLimit(1)
                        .truncationMode(.middle)
                        .monospacedDigit()
                }
            }
            .frame(minHeight: 100)
        }
    }

    private func byteText(_ bytes: UInt64) -> String {
        ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)
    }

    private func sizeText(_ url: URL) -> String {
        byteText(UInt64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0))
    }

    private func choose() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = true
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        panel.prompt = DeskhubClient.string(DHStrTransferChooseButton)
        guard panel.runModal() == .OK else { return }
        model.transferError = ""
        model.chosenFiles = Array(panel.urls.prefix(DeskhubClient.maxTransferFiles))
        if panel.urls.count > model.chosenFiles.count {
            model.transferError = DeskhubClient.string(DHStrTransferTooManyFiles)
        }
    }
}
