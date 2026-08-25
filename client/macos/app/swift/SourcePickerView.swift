import SwiftUI

struct SourcePickerView: View {
    let sources: [Source]
    let onCancel: () -> Void
    let onPick: ([Source]) -> Void

    @State private var picked: Set<UInt8> = []

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            List {
                ForEach(sources) { source in
                    HStack {
                        Text(source.pickerLabel)
                        Spacer()
                    }
                    .contentShape(Rectangle())
                    .listRowBackground(picked.contains(source.id)
                        ? Color.accentColor.opacity(0.25) : Color.clear)
                    .onTapGesture { toggle(source.id) }
                    .simultaneousGesture(TapGesture(count: 2).onEnded {
                        picked.insert(source.id)
                        view()
                    })
                }
            }
            .listStyle(.bordered)
            .frame(maxWidth: .infinity, maxHeight: .infinity)

            Text(DeskhubClient.string(DHStrPickerEachWindow))

            HStack {
                Spacer()
                Button("View") { view() }
                    .buttonStyle(.borderedProminent)
                    .disabled(picked.isEmpty)
                Button("Cancel") { onCancel() }
            }
        }
        .padding(12)
        .onAppear {
            if picked.isEmpty, let first = sources.first { picked = [first.id] }
        }
    }

    private func toggle(_ id: UInt8) {
        if picked.contains(id) { picked.remove(id) } else { picked.insert(id) }
    }

    private func view() {
        let chosen = sources.filter { picked.contains($0.id) }
        guard !chosen.isEmpty else { return }
        onPick(chosen)
    }
}
