import SwiftUI

struct IosShellPickerView: View {
    @Bindable var model: TerminalModel
    let onClose: () -> Void

    @State private var closing: ShellRow?

    private var closeShellTitle: String { DeskhubClient.string(DHStrShellPickerClose) }

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack(spacing: 12) {
                Text(DeskhubClient.string(DHStrShellPickerTitle))
                    .font(.system(size: 16, weight: .medium))
                    .foregroundStyle(.white)
                    .frame(maxWidth: .infinity, alignment: .leading)
                SessionCloseButton(action: onClose)
            }

            if model.shells.isEmpty {
                Text(DeskhubClient.string(DHStrShellPickerEmpty))
                    .foregroundStyle(.gray)
            }

            ScrollView {
                VStack(spacing: 8) {
                    ForEach(model.shells) { row in
                        shellRow(row)
                    }
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            Button {
                model.openFreshShell()
            } label: {
                Text(DeskhubClient.string(DHStrShellPickerNew))
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.deskhubOutlined())
        }
        .padding(16)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .background(Color.black.ignoresSafeArea())
        .alert(
            closeShellTitle,
            isPresented: Binding(get: { closing != nil }, set: { if !$0 { closing = nil } }),
            presenting: closing
        ) { row in
            Button(closeShellTitle) {
                model.closeShell(row.id)
                closing = nil
            }
            Button(DeskhubClient.string(DHStrCancelAction), role: .cancel) { closing = nil }
        } message: { _ in
            Text(DeskhubClient.string(DHStrShellPickerCloseAsk))
        }
    }

    private func shellRow(_ row: ShellRow) -> some View {
        HStack(spacing: 8) {
            Button {
                model.resumeShell(row.id)
            } label: {
                Text(row.line)
                    .frame(maxWidth: .infinity)
            }
            .buttonStyle(.deskhubOutlined())
            .disabled(!row.resumable)

            if row.closable {
                SessionTextButton(closeShellTitle) { closing = row }
            }
        }
    }
}
