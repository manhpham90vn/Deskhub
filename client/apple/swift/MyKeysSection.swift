import SwiftUI
import UniformTypeIdentifiers

struct MyKeysSection: View {
    let model: ClientKeysModel

    @State private var askingNewKeyName = false
    @State private var choosingImportFile = false
    @State private var askingImportDetails = false
    @State private var keyName = ""
    @State private var passphrase = ""
    @State private var importedKeyText = ""

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            deskhubSection(DeskhubClient.string(DHStrMyKeysHeading))
            deskhubHint(DeskhubClient.string(DHStrMyKeysHint))

            ForEach(model.keys) { key in
                keyRow(key)
            }

            HStack(spacing: 10) {
                Button(DeskhubClient.string(DHStrNewKeyAction)) {
                    keyName = ""
                    askingNewKeyName = true
                }
                Button(DeskhubClient.string(DHStrImportKeyAction)) {
                    choosingImportFile = true
                }
            }
            .buttonStyle(.bordered)

            if !model.error.isEmpty {
                Text(model.error).foregroundStyle(DeskhubPalette.offline)
            }
        }
        .alert(DeskhubClient.string(DHStrNewKeyAction), isPresented: $askingNewKeyName) {
            keyNameField
            Button("OK") { model.generate(name: keyName) }
            Button(DeskhubClient.string(DHStrCancelAction), role: .cancel) {}
        }
        .fileImporter(
            isPresented: $choosingImportFile,
            allowedContentTypes: [.item],
            allowsMultipleSelection: false
        ) { result in
            readImportFile(result)
        }
        .alert(DeskhubClient.string(DHStrImportKeyAction), isPresented: $askingImportDetails) {
            keyNameField
            SecureField(DeskhubClient.string(DHStrKeyPassphraseLabel), text: $passphrase)
            Button("OK", action: importChosenKey)
            Button(DeskhubClient.string(DHStrCancelAction), role: .cancel, action: forgetImport)
        }
    }

    private var keyNameField: some View {
        TextField(DeskhubClient.string(DHStrKeyNameLabel), text: $keyName)
            .autocorrectionDisabled()
    }

    private func keyRow(_ key: ClientKeyRow) -> some View {
        HStack(spacing: 12) {
            VStack(alignment: .leading, spacing: 2) {
                Text(key.name).foregroundStyle(DeskhubPalette.heading)
                Text(key.shortFingerprint)
                    .font(.system(.caption, design: .monospaced))
                    .foregroundStyle(DeskhubPalette.muted)
                    .help(key.fingerprint)
                    .accessibilityValue(key.fingerprint)
            }
            Spacer(minLength: 0)
            CopyButton(DeskhubClient.string(DHStrCopyPublicKeyAction)) {
                model.publicKey(of: key)
            }
            .buttonStyle(.bordered)
            .controlSize(.small)
        }
        .padding(.vertical, 2)
    }

    private func readImportFile(_ result: Result<[URL], Error>) {
        guard case let .success(urls) = result, let url = urls.first else { return }
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        guard let text = try? String(contentsOf: url, encoding: .utf8) else {
            model.reportUnreadable()
            return
        }
        importedKeyText = text
        keyName = url.deletingPathExtension().lastPathComponent
        passphrase = ""
        askingImportDetails = true
    }

    private func importChosenKey() {
        model.importKey(name: keyName, privateKey: importedKeyText, passphrase: passphrase)
        forgetImport()
    }

    private func forgetImport() {
        importedKeyText = ""
        passphrase = ""
    }
}
