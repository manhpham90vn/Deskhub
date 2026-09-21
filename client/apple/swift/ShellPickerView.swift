import SwiftUI

struct ShellPickerView: View {
    @Bindable var model: TerminalModel
    @State private var closing: ShellRow?

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(DeskhubClient.string(DHStrShellPickerTitle))
                .font(.system(size: 17, weight: .semibold))
                .foregroundStyle(.white)

            if model.shells.isEmpty {
                Text(DeskhubClient.string(DHStrShellPickerEmpty))
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }

            ScrollView {
                VStack(spacing: 8) {
                    ForEach(model.shells) { row in
                        HStack(spacing: 12) {
                            Button { model.resumeShell(row.id) } label: {
                                Text(row.line)
                                    .font(.system(size: 14, design: .monospaced))
                                    .frame(maxWidth: .infinity, alignment: .leading)
                            }
                            .disabled(!row.resumable)

                            if row.closable {
                                Button(role: .destructive) { closing = row } label: {
                                    Text(DeskhubClient.string(DHStrShellPickerClose))
                                }
                            }
                        }
                    }
                }
            }

            Button(DeskhubClient.string(DHStrShellPickerNew)) { model.openFreshShell() }
                .deskhubPrimaryLabel()
        }
        .padding(16)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
        .confirmationDialog(
            DeskhubClient.string(DHStrShellPickerCloseAsk),
            isPresented: Binding(get: { closing != nil }, set: { if !$0 { closing = nil } }),
            titleVisibility: .visible
        ) {
            Button(DeskhubClient.string(DHStrShellPickerClose), role: .destructive) {
                if let row = closing { model.closeShell(row.id) }
                closing = nil
            }
        }
    }
}
