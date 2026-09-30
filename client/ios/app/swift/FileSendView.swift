import PhotosUI
import SwiftUI
import UniformTypeIdentifiers

private let stagingFolderName = "deskhub-send"
private let fileRowFontSize: CGFloat = 12

private func stagingDirectory() throws -> URL {
    let dir = FileManager.default.temporaryDirectory
        .appendingPathComponent(stagingFolderName, isDirectory: true)
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    return dir
}

private func freeName(in dir: URL, for name: String) -> URL {
    let base = (name as NSString).deletingPathExtension
    let ext = (name as NSString).pathExtension
    var candidate = dir.appendingPathComponent(name.isEmpty ? "file" : name)
    var attempt = 1
    while FileManager.default.fileExists(atPath: candidate.path) {
        attempt += 1
        let numbered = ext.isEmpty ? "\(base)-\(attempt)" : "\(base)-\(attempt).\(ext)"
        candidate = dir.appendingPathComponent(numbered)
    }
    return candidate
}

func stageForSending(_ source: URL) throws -> URL {
    let dir = try stagingDirectory()
    let target = freeName(in: dir, for: source.lastPathComponent)
    try FileManager.default.copyItem(at: source, to: target)
    return target
}

func clearSendStaging() {
    guard let dir = try? stagingDirectory() else { return }
    try? FileManager.default.removeItem(at: dir)
}

private struct PickedPhoto: Transferable {
    let url: URL

    static var transferRepresentation: some TransferRepresentation {
        FileRepresentation(importedContentType: .item) { received in
            try PickedPhoto(url: stageForSending(received.file))
        }
    }
}

private struct FileRow: View {
    let name: String
    let detail: String

    var body: some View {
        HStack(spacing: 8) {
            Text(name)
                .lineLimit(1)
                .truncationMode(.middle)
                .frame(maxWidth: .infinity, alignment: .leading)
            Text(detail)
                .lineLimit(1)
                .monospacedDigit()
        }
        .font(.system(size: fileRowFontSize))
    }
}

struct FileSendView<Driver: TransferDriver>: View {
    let model: Driver
    let subtitle: String
    let onClose: () -> Void

    @State private var photoItems: [PhotosPickerItem] = []
    @State private var browsingFiles = false
    @State private var staging = false

    private var pickersDisabled: Bool { model.transfer.active || staging }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 12) {
                    Text(DeskhubClient.string(DHStrTransferSendHeading))
                        .font(.system(size: 22, weight: .bold))
                        .frame(maxWidth: .infinity, alignment: .leading)
                    SessionCloseButton(action: close, enabled: !model.transfer.active)
                }

                Text(subtitle)
                    .font(.caption)
                    .foregroundStyle(DeskhubPalette.muted)
                    .lineLimit(1)
                    .truncationMode(.middle)

                pickers
                chosenList

                if !model.transferError.isEmpty {
                    Text(model.transferError)
                        .foregroundStyle(.red)
                        .fixedSize(horizontal: false, vertical: true)
                }

                sendButton

                if !model.transfer.idle {
                    progress
                }

                sent
            }
            .padding(16)
        }
        .onChange(of: photoItems) { _, picked in
            guard !picked.isEmpty else { return }
            Task { await stagePhotos(picked) }
        }
        .fileImporter(
            isPresented: $browsingFiles,
            allowedContentTypes: [.item],
            allowsMultipleSelection: true
        ) { result in
            stageBrowsedFiles(result)
        }
    }

    private var pickers: some View {
        HStack(spacing: 8) {
            PhotosPicker(
                selection: $photoItems,
                maxSelectionCount: DeskhubClient.maxTransferFiles,
                matching: .any(of: [.images, .videos])
            ) {
                Text("Photos")
            }
            .buttonStyle(.deskhubOutlined())
            .simultaneousGesture(TapGesture().onEnded { model.transferError = "" })
            .disabled(pickersDisabled)

            Button("Files") {
                model.transferError = ""
                browsingFiles = true
            }
            .buttonStyle(.deskhubOutlined())
            .disabled(pickersDisabled)
        }
    }

    @ViewBuilder
    private var chosenList: some View {
        if model.chosenFiles.isEmpty {
            Text(DeskhubClient.string(DHStrTransferNoneChosen))
        } else {
            VStack(alignment: .leading, spacing: 2) {
                ForEach(model.chosenFiles, id: \.self) { url in
                    FileRow(name: url.lastPathComponent, detail: sizeText(url))
                }
            }
        }
    }

    private var progress: some View {
        VStack(alignment: .leading, spacing: 8) {
            ProgressView(value: model.transfer.fraction)
            FileRow(name: statusText, detail: model.transfer.step)
            if model.transfer.active {
                Button {
                    model.cancelTransfer()
                } label: {
                    Text(DeskhubClient.string(DHStrTransferCancelButton))
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.deskhubOutlined())
            }
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
            Text(DeskhubClient.string(DHStrTransferSentHeading))
                .font(.system(size: 12, weight: .medium))
            VStack(alignment: .leading, spacing: 2) {
                ForEach(model.history) { row in
                    FileRow(
                        name: (row.ok ? "\u{2713} " : "\u{2715} ") + row.name,
                        detail: row.ok ? byteText(row.bytes) : row.detail
                    )
                }
            }
        }
    }

    private var sendButton: some View {
        Button {
            model.sendChosenFiles()
        } label: {
            Text("Send")
                .font(.system(size: 15, weight: .semibold))
                .frame(maxWidth: .infinity, minHeight: 28)
        }
        .buttonStyle(.borderedProminent)
        .tint(DeskhubPalette.accent)
        .disabled(!model.canSend || staging)
    }

    private func stagePhotos(_ picked: [PhotosPickerItem]) async {
        staging = true
        model.transferError = ""
        var staged: [URL] = []
        for item in picked.prefix(DeskhubClient.maxTransferFiles) {
            guard let photo = try? await item.loadTransferable(type: PickedPhoto.self) else {
                continue
            }
            staged.append(photo.url)
        }
        photoItems = []
        staging = false
        adopt(staged, asked: picked.count)
    }

    private func stageBrowsedFiles(_ result: Result<[URL], Error>) {
        guard case let .success(urls) = result, !urls.isEmpty else { return }
        model.transferError = ""
        var staged: [URL] = []
        for url in urls.prefix(DeskhubClient.maxTransferFiles) {
            let scoped = url.startAccessingSecurityScopedResource()
            defer { if scoped { url.stopAccessingSecurityScopedResource() } }
            guard let copy = try? stageForSending(url) else { continue }
            staged.append(copy)
        }
        adopt(staged, asked: urls.count)
    }

    private func adopt(_ staged: [URL], asked: Int) {
        guard !staged.isEmpty else { return }
        model.chosenFiles = staged
        if asked > staged.count {
            model.transferError = DeskhubClient.string(DHStrTransferTooManyFiles)
        }
    }

    private func byteText(_ bytes: UInt64) -> String {
        ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)
    }

    private func sizeText(_ url: URL) -> String {
        byteText(UInt64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0))
    }

    private func close() {
        model.forgetTransfer()
        clearSendStaging()
        onClose()
    }
}
