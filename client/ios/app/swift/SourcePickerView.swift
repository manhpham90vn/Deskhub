import SwiftUI

struct SourcePickerView: View {
    @Bindable var model: AppModel
    let sources: [Source]

    @State private var picked: UInt8?

    private var selectedId: UInt8 { picked ?? sources.first?.id ?? 0 }

    var body: some View {
        VStack(spacing: 0) {
            HStack {
                Spacer(minLength: 0)
                Button(DeskhubClient.string(DHStrCancelAction), action: model.closeSourcePicker)
                    .buttonStyle(.iosText())
            }
            .padding(.horizontal, 8)

            ScrollView {
                VStack(alignment: .leading, spacing: 8) {
                    Text(model.connect.acceptedAddress)
                        .font(.system(size: iosSectionSize, weight: .medium))
                        .foregroundStyle(DeskhubPalette.heading)

                    ForEach(sources) { source in
                        sourceRow(source)
                    }
                }
                .padding(iosPagePadding)
            }

            Button("Start viewing") { model.startStream(sourceId: selectedId) }
                .buttonStyle(.iosFilled(fullWidth: true))
                .disabled(sources.isEmpty)
                .padding(iosPagePadding)
        }
        .background { DeskhubPalette.page.ignoresSafeArea() }
    }

    private func sourceRow(_ source: Source) -> some View {
        Button {
            picked = source.id
        } label: {
            HStack(spacing: 12) {
                Image(systemName: source.id == selectedId ? "largecircle.fill.circle" : "circle")
                    .font(.system(size: 20))
                    .foregroundStyle(DeskhubPalette.accent)
                VStack(alignment: .leading, spacing: 2) {
                    Text(source.displayName)
                        .font(.system(size: iosSectionSize))
                        .foregroundStyle(DeskhubPalette.heading)
                    iosHint(source.sizeLabel)
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 8)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
