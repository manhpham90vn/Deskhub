import PhotosUI
import SwiftUI
import UniformTypeIdentifiers

private let stagingFolderName = "deskhub-send"

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

struct FileSendView<Driver: TransferDriver>: View {
    let model: Driver
    let subtitle: String
    let onClose: () -> Void

    @State private var photoItems: [PhotosPickerItem] = []
    @State private var browsingFiles = false
    @State private var staging = false

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                deskhubHeading(DeskhubClient.string(DHStrTransferSendHeading))
                Spacer(minLength: 0)
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
                    .font(.callout)
                    .foregroundStyle(.red)
                    .fixedSize(horizontal: false, vertical: true)
            }

            sendButton

            if !model.transfer.idle {
                progress
            }

            sent
            Spacer(minLength: 0)
        }
        .padding()
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
        HStack(spacing: 10) {
            PhotosPicker(
                selection: $photoItems,
                maxSelectionCount: DeskhubClient.maxTransferFiles,
                matching: .any(of: [.images, .videos])
            ) {
                Label("Photos", systemImage: "photo.on.rectangle")
            }
            .buttonStyle(.bordered)
            .disabled(model.transfer.active || staging)

            Button {
                model.transferError = ""
                browsingFiles = true
            } label: {
                Label("Files", systemImage: "folder")
            }
            .buttonStyle(.bordered)
            .disabled(model.transfer.active || staging)

            if staging {
                ProgressView()
            }
        }
    }

    @ViewBuilder
    private var chosenList: some View {
        if model.chosenFiles.isEmpty {
            Text(DeskhubClient.string(DHStrTransferNoneChosen))
                .foregroundStyle(DeskhubPalette.muted)
                .frame(maxWidth: .infinity, alignment: .leading)
        } else {
            List(model.chosenFiles, id: \.self) { url in
                HStack {
                    Text(url.lastPathComponent).lineLimit(1).truncationMode(.middle)
                    Spacer()
                    Text(sizeText(url)).foregroundStyle(DeskhubPalette.muted).monospacedDigit()
                }
            }
            .listStyle(.plain)
            .frame(height: 160)
        }
    }

    private var progress: some View {
        VStack(alignment: .leading, spacing: 8) {
            ProgressView(value: model.transfer.fraction)
            HStack {
                Text(statusText).lineLimit(1).truncationMode(.middle)
                Spacer()
                Text(model.transfer.step).monospacedDigit()
                    .foregroundStyle(DeskhubPalette.muted)
            }
            .font(.callout)

            if model.transfer.active {
                Button(DeskhubClient.string(DHStrTransferCancelButton)) {
                    model.cancelTransfer()
                }
                .buttonStyle(.bordered)
                .frame(maxWidth: .infinity)
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
            Divider()
            Text(DeskhubClient.string(DHStrTransferSentHeading))
                .font(.subheadline)
                .foregroundStyle(DeskhubPalette.muted)
            List(model.history) { row in
                HStack(spacing: 8) {
                    Image(systemName: row.ok ? "checkmark.circle" : "xmark.circle")
                        .foregroundStyle(row.ok ? DeskhubPalette.online : DeskhubPalette.offline)
                    Text(row.name).lineLimit(1).truncationMode(.middle)
                    Spacer()
                    Text(row.ok ? byteText(row.bytes) : row.detail)
                        .foregroundStyle(DeskhubPalette.muted)
                        .lineLimit(1)
                        .truncationMode(.middle)
                }
            }
            .listStyle(.plain)
            .frame(minHeight: 120)
        }
    }

    private var sendButton: some View {
        Button("Send") { model.sendChosenFiles() }
            .buttonStyle(.borderedProminent)
            .controlSize(.large)
            .tint(DeskhubPalette.accent)
            .frame(maxWidth: .infinity)
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
