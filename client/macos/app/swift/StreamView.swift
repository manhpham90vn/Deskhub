import AppKit
import AVFoundation
import SwiftUI

struct ViewerRequest: Codable, Hashable {
    var address: String
    var passcode: String
    var sourceId: UInt8
    var name: String
    var control: Bool
}

struct ViewerWindow: View {
    @State private var model: StreamModel
    @Environment(\.dismiss) private var dismiss
    @Environment(\.openWindow) private var openWindow

    init(request: ViewerRequest) {
        _model = State(initialValue: StreamModel(
            address: request.address,
            passcode: request.passcode,
            sourceId: request.sourceId,
            sourceName: request.name,
            control: request.control
        ))
    }

    var body: some View {
        StreamView(model: model) { closeWindow() }
            .navigationTitle(title)
            .navigationSubtitle(subtitle)
            .frame(minWidth: 640, idealWidth: 1024, maxWidth: .infinity,
                   minHeight: 400, idealHeight: 634, maxHeight: .infinity)
            .task {
                dh_viewer_opened()
                await model.start()
            }
            .task {
                guard dh_clipboard_sync() else { return }
                var lastChange = NSPasteboard.general.changeCount
                while !Task.isCancelled {
                    try? await Task.sleep(for: .seconds(1))
                    let board = NSPasteboard.general
                    if let remote = model.takeClipboard() {
                        board.clearContents()
                        board.setString(remote, forType: .string)
                        lastChange = board.changeCount
                        continue
                    }
                    guard board.changeCount != lastChange else { continue }
                    lastChange = board.changeCount
                    if let text = board.string(forType: .string), !text.isEmpty {
                        model.offerClipboard(text)
                    }
                }
            }
            .onDisappear {
                model.setLayer(nil)
                model.disconnect()
                if dh_viewer_closed() { openWindow(id: "main") }
            }
            .alert("Deskhub", isPresented: failedShown) {
                Button("OK") { closeWindow() }
            } message: {
                Text(DeskhubClient.string(DHStrViewerOpenFailed))
            }
    }

    private func closeWindow() {
        DispatchQueue.main.async { dismiss() }
    }

    private var title: String {
        DeskhubClient.viewerBaseTitle(model.sourceName)
    }

    private var subtitle: String {
        model.control
            ? DeskhubClient.pointerSubtitle(locked: model.mouseLocked,
                                            statusLine: model.statusLine)
            : DeskhubClient.viewOnlySubtitle(statusLine: model.statusLine)
    }

    private var failedShown: Binding<Bool> {
        Binding(
            get: { model.failedToStart },
            set: { shown in
                if !shown {
                    model.failedToStart = false
                    model.endReason = ""
                }
            }
        )
    }
}

struct StreamView: View {
    @Bindable var model: StreamModel
    let onEnd: () -> Void

    var body: some View {
        VStack(spacing: 0) {
            ConnectionStatusBar(model: model, onDisconnect: onEnd)
            RemoteView(
                model: model,
                videoSize: videoSize,
                mouseLocked: model.mouseLocked,
                onLayerReady: { layer in model.setLayer(layer) },
                onLockChanged: { model.mouseLocked = $0 }
            )
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color.black)
            .environment(\.colorScheme, .dark)
        }
        .alert("Deskhub", isPresented: endedAlertShown) {
            Button("OK") { onEnd() }
        } message: {
            Text("\(DeskhubClient.string(DHStrConnectionEndedTitle)): \(model.endReason)")
        }
    }

    private var videoSize: CGSize {
        CGSize(width: Double(model.videoWidth), height: Double(model.videoHeight))
    }

    private var endedAlertShown: Binding<Bool> {
        Binding(
            get: { !model.endReason.isEmpty && !model.failedToStart },
            set: { shown in
                if !shown {
                    model.endReason = ""
                }
            }
        )
    }
}
