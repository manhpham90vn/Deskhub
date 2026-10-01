import SwiftUI

struct StatusOverlay: View {
    let model: StreamModel
    let streaming: Bool
    let onBack: () -> Void

    var body: some View {
        if model.phase == .ended {
            ended
        } else if model.reattaching {
            SessionBanner(text: DeskhubClient.string(DHStrLinkReattaching))
        } else if !streaming {
            connecting
        }
    }

    private var connecting: some View {
        VStack(spacing: 12) {
            ProgressView()
            Text(DeskhubClient.connectingTo(model.address))
                .foregroundStyle(.white)
        }
    }

    private var ended: some View {
        VStack(spacing: 12) {
            Text(DeskhubClient.string(DHStrSessionEnded))
                .font(.headline)
                .foregroundStyle(.white)
            Text(model.endReason)
                .foregroundStyle(.white)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
            SessionTextButton("Back", action: onBack)
        }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .contentShape(Rectangle())
        .onTapGesture(perform: onBack)
    }
}

struct ZoomControls: View {
    let zoom: CGFloat
    let panMode: Bool
    let onToggleMode: () -> Void
    let onReset: () -> Void

    var body: some View {
        VStack(alignment: .trailing, spacing: 8) {
            pill(panMode ? "Pan" : "Pointer", action: onToggleMode)
                .accessibilityLabel(
                    panMode ? "One finger pans the view" : "One finger moves the pointer"
                )
            pill(DeskhubClient.zoomLabel(Double(zoom)), action: onReset)
                .accessibilityLabel("Reset zoom")
        }
    }

    private func pill(_ text: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(text)
                .font(.caption.weight(.semibold))
                .foregroundStyle(.white)
                .padding(.horizontal, 10)
                .padding(.vertical, 6)
                .background(.black.opacity(0.45), in: Capsule())
                .overlay(Capsule().strokeBorder(.white.opacity(0.25), lineWidth: 1))
        }
        .buttonStyle(.plain)
    }
}

struct StreamControlPanel: View {
    let session: AppModel
    let model: StreamModel
    let streaming: Bool
    @Binding var keyboardOn: Bool
    @Binding var displayPickerOpen: Bool
    @State private var isOpen = false

    var body: some View {
        if isOpen {
            panel
        } else {
            SessionExpandButton {
                withAnimation(.easeOut(duration: 0.18)) { isOpen = true }
            }
        }
    }

    private var hostTitle: String {
        DeskhubClient.hostTitle(
            model.address, width: model.videoWidth, height: model.videoHeight
        )
    }

    private var panel: some View {
        VStack(alignment: .leading, spacing: 6) {
            header
            hotkeyStrip
            actions
        }
        .padding(12)
        .frame(maxWidth: .infinity)
        .background(sessionBannerFill, in: RoundedRectangle(cornerRadius: 16))
    }

    private var header: some View {
        HStack(alignment: .top, spacing: 8) {
            VStack(alignment: .leading, spacing: 2) {
                Text(hostTitle)
                    .font(.caption)
                    .foregroundStyle(.white)
                    .lineLimit(1)
                    .truncationMode(.middle)
                if streaming, !model.statusLine.isEmpty {
                    Text(model.statusLine)
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.8))
                        .lineLimit(1)
                        .minimumScaleFactor(0.75)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            Button {
                withAnimation(.easeOut(duration: 0.18)) { isOpen = false }
            } label: {
                Text(verbatim: "\u{2304}")
                    .font(.system(size: 17))
                    .foregroundStyle(.white)
                    .frame(width: 32, height: 32)
                    .contentShape(Circle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("Hide controls")
        }
    }

    private var hotkeyStrip: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                ForEach(kHotkeys, id: \.label) { hotkey in
                    Button(hotkey.label) { model.hotkey(hotkey) }
                        .buttonStyle(.deskhubOutlined(horizontalPadding: 12, verticalPadding: 6))
                        .disabled(!streaming)
                }
            }
            .padding(.vertical, 1)
        }
    }

    private var actions: some View {
        HStack(spacing: 8) {
            Button(keyboardOn ? "Hide keyboard" : "Keyboard") { keyboardOn.toggle() }
                .buttonStyle(.deskhubOutlined())
                .disabled(!streaming)

            if session.sources.count > 1 {
                Button("Display") { displayPickerOpen = true }
                    .buttonStyle(.deskhubOutlined())
            }

            Spacer(minLength: 0)
        }
    }
}

struct DisplayPickerDialog: View {
    let sources: [Source]
    let currentSourceId: UInt8
    let onPick: (UInt8) -> Void
    let onDismiss: () -> Void

    var body: some View {
        ZStack {
            Color.black.opacity(0.5)
                .ignoresSafeArea()
                .onTapGesture(perform: onDismiss)

            VStack(alignment: .leading, spacing: 16) {
                Text("Display")
                    .font(.system(size: 22, weight: .semibold))
                    .foregroundStyle(.white)

                VStack(alignment: .leading, spacing: 0) {
                    ForEach(sources) { source in
                        row(source)
                    }
                }

                HStack {
                    Spacer(minLength: 0)
                    SessionTextButton("Cancel", action: onDismiss)
                }
            }
            .padding(24)
            .frame(maxWidth: 360)
            .background(Color(white: 0.16), in: RoundedRectangle(cornerRadius: 28))
            .padding(.horizontal, 32)
        }
    }

    private func row(_ source: Source) -> some View {
        Button {
            onPick(source.id)
            onDismiss()
        } label: {
            HStack(spacing: 12) {
                Image(systemName: source.id == currentSourceId ? "largecircle.fill.circle" : "circle")
                    .font(.system(size: 20))
                    .foregroundStyle(
                        source.id == currentSourceId ? DeskhubPalette.accent : Color.white.opacity(0.7)
                    )
                VStack(alignment: .leading, spacing: 2) {
                    Text(source.displayName)
                        .foregroundStyle(.white)
                    Text(source.sizeLabel)
                        .font(.caption)
                        .foregroundStyle(.white.opacity(0.7))
                }
                Spacer(minLength: 0)
            }
            .padding(.vertical, 8)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
}
