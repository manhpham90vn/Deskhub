import AVFoundation
import SwiftUI
import UIKit

struct StreamView: View {
    @Bindable var session: AppModel
    @Bindable var model: StreamModel
    @Environment(\.scenePhase) private var scenePhase
    @State private var layer: AVSampleBufferDisplayLayer?
    @State private var keyboardOn = false

    @State private var controlsOpen = false

    @State private var controlsRect: CGRect = .zero

    @State private var closeRect: CGRect = .zero

    @State private var transform = ViewTransform()

    @State private var panMode = false

    private var streaming: Bool { model.phase == .streaming }

    var body: some View {
        GeometryReader { proxy in
            let safeArea = proxy.safeAreaInsets
            ZStack(alignment: .bottomTrailing) {
                videoArea
                    .padding(.leading, safeArea.leading)
                    .padding(.trailing, safeArea.trailing)
                StatusOverlay(model: model, streaming: streaming, onBack: session.disconnect)
                    .frame(maxWidth: .infinity, maxHeight: .infinity)
                    .padding(safeArea)
                SessionCloseButton(action: session.disconnect)
                    .background(
                        GeometryReader { proxy in
                            Color.clear
                                .onChange(of: proxy.frame(in: .global), initial: true) { _, rect in
                                    closeRect = rect
                                }
                        }
                    )
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topTrailing)
                    .padding(safeArea)
                    .padding(12)
                controlsLayer
                    .padding(safeArea)
            }
            .background(Color.black)
            .ignoresSafeArea()
        }
        .onChange(of: scenePhase) { _, newPhase in
            switch newPhase {
            case .background:
                model.releaseAllInput()
                releaseLayer()
            case .active:
                if let layer {
                    model.setLayer(layer)
                }
            case .inactive:
                break
            @unknown default:
                break
            }
        }
        .onChange(of: model.sourceId) { _, _ in
            transform = ViewTransform()
        }
        .onChange(of: transform.isZoomed) { _, zoomed in
            panMode = zoomed
        }
        .onAppear {
            UIApplication.shared.isIdleTimerDisabled = dh_keep_awake()
            model.refresh()
        }
        .task {
            guard dh_clipboard_sync() else { return }
            var lastChange = UIPasteboard.general.changeCount
            while !Task.isCancelled {
                try? await Task.sleep(for: .seconds(1))
                guard streaming, scenePhase == .active else { continue }
                let board = UIPasteboard.general
                if let remote = model.takeClipboard() {
                    board.string = remote
                    lastChange = board.changeCount
                    continue
                }
                guard board.changeCount != lastChange else { continue }
                lastChange = board.changeCount
                if board.hasStrings, let text = board.string, !text.isEmpty {
                    model.offerClipboard(text)
                }
            }
        }
        .onDisappear {
            UIApplication.shared.isIdleTimerDisabled = false
            keyboardOn = false
            releaseLayer()
        }
        .statusBarHidden()
    }

    private var videoArea: some View {
        GeometryReader { proxy in
            let viewport = proxy.size
            let frame = transform.frame(in: viewport, aspect: aspectRatio)
            let base = ViewTransform.baseFrame(in: viewport, aspect: aspectRatio)
            ZStack(alignment: .topLeading) {
                VideoLayerView { newLayer in
                    layer = newLayer
                    model.setLayer(newLayer)
                }
                .frame(width: max(base.width, 1), height: max(base.height, 1))
                .scaleEffect(transform.zoom, anchor: .topLeading)
                .offset(x: frame.minX, y: frame.minY)

                if streaming {
                    TouchInputView(
                        model: model,
                        videoRect: frame,
                        blockedRects: [controlsRect, closeRect],
                        panMode: panMode,
                        zoomed: transform.isZoomed,
                        onTransform: { factor, centroid, panDelta in
                            transform.apply(
                                factor: factor, centroid: centroid, panDelta: panDelta,
                                viewport: viewport, aspect: aspectRatio
                            )
                        }
                    )
                    .frame(width: viewport.width, height: viewport.height)
                }

                KeyInputView(model: model, active: $keyboardOn)
                    .frame(width: 1, height: 1)
                    .opacity(0)
                    .allowsHitTesting(false)
            }
            .frame(width: viewport.width, height: viewport.height)
            .clipped()
        }
    }

    private var aspectRatio: CGFloat {
        CGFloat(model.aspectRatio)
    }

    private var controlsLayer: some View {
        VStack(alignment: .trailing, spacing: 8) {
            if transform.isZoomed {
                ZoomControls(
                    zoom: transform.zoom,
                    panMode: panMode,
                    onToggleMode: { panMode.toggle() },
                    onReset: {
                        withAnimation(.easeOut(duration: 0.18)) { transform = ViewTransform() }
                    }
                )
            }
            StreamControlPanel(
                session: session,
                model: model,
                streaming: streaming,
                isOpen: $controlsOpen,
                keyboardOn: $keyboardOn
            )
        }
        .padding(12)
        .background(
            GeometryReader { proxy in
                Color.clear
                    .onChange(of: proxy.frame(in: .global), initial: true) { _, rect in
                        controlsRect = rect
                    }
            }
        )
    }

    private func releaseLayer() {
        model.setLayer(nil)
    }
}
