import AppKit
import AVFoundation
import SwiftUI

final class RemoteVideoView: NSView {
    weak var model: StreamModel?

    var videoSize: CGSize = .zero {
        didSet {
            needsLayout = true
            refitWindow()
        }
    }

    private var fittedSize: CGSize = .zero

    private var pointer = DHPointerLock(locked: false)
    var mouseLocked: Bool { pointer.locked }
    var onLockChanged: ((Bool) -> Void)?

    private var trackingArea: NSTrackingArea?
    private var lastFlags: NSEvent.ModifierFlags = []
    private var scrollCarry: Double = 0

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true
        layer?.backgroundColor = NSColor.black.cgColor
    }

    @available(*, unavailable)
    required init?(coder _: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func makeBackingLayer() -> CALayer {
        let videoLayer = AVSampleBufferDisplayLayer()
        videoLayer.videoGravity = .resizeAspect
        videoLayer.backgroundColor = NSColor.black.cgColor
        return videoLayer
    }

    var displayLayer: AVSampleBufferDisplayLayer? {
        layer as? AVSampleBufferDisplayLayer
    }

    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }

    override func acceptsFirstMouse(for _: NSEvent?) -> Bool { true }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        if let trackingArea { removeTrackingArea(trackingArea) }
        let area = NSTrackingArea(
            rect: .zero,
            options: [.mouseMoved, .mouseEnteredAndExited, .activeInKeyWindow, .inVisibleRect],
            owner: self,
            userInfo: nil
        )
        addTrackingArea(area)
        trackingArea = area
    }

    private func refitWindow() {
        guard let window, let screen = window.screen ?? NSScreen.main else { return }
        let newW = UInt32(max(0, videoSize.width))
        let newH = UInt32(max(0, videoSize.height))
        guard dh_should_refit_viewer(
            UInt32(max(0, fittedSize.width)), UInt32(max(0, fittedSize.height)), newW, newH
        ) else { return }
        fittedSize = videoSize
        var fitW: UInt32 = 0
        var fitH: UInt32 = 0
        dh_fit_viewer_window(
            newW, newH,
            UInt32(max(0, screen.visibleFrame.width)),
            UInt32(max(0, screen.visibleFrame.height)),
            &fitW, &fitH
        )
        guard fitW > 0, fitH > 0 else { return }
        window.setContentSize(NSSize(width: Double(fitW), height: Double(fitH)))
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        refitWindow()
    }

    private var videoRect: CGRect {
        guard videoSize.width > 0, videoSize.height > 0 else { return bounds }
        let rect = ViewTransform.baseFrame(
            in: bounds.size, aspect: videoSize.width / videoSize.height
        )
        return rect.isEmpty ? bounds : rect
    }

    private func normalize(_ point: NSPoint) -> (nx: Int32, ny: Int32)? {
        var nx: Int32 = 0
        var ny: Int32 = 0
        guard dh_normalize_pointer(
            Double(point.x), Double(point.y), videoRect.dhRect, &nx, &ny
        ) else {
            return nil
        }
        return (nx, ny)
    }

    private func apply(_ effect: DHPointerLockEffect, notify: Bool = true) {
        if effect.releaseHeldInput { model?.releaseAllInput() }
        if effect.lockChanged { grabPointer(pointer.locked) }
        if notify, effect.lockChanged { onLockChanged?(pointer.locked) }
    }

    private func grabPointer(_ on: Bool) {
        if on {
            CGAssociateMouseAndMouseCursorPosition(0)
            NSCursor.hide()
        } else {
            CGAssociateMouseAndMouseCursorPosition(1)
            NSCursor.unhide()
        }
    }

    func setMouseLocked(_ on: Bool, notify: Bool = true) {
        guard pointer.locked != on else { return }
        apply(dh_pointer_toggle_lock(&pointer), notify: notify)
    }

    override func keyDown(with event: NSEvent) {
        if isLockToggle(event.keyCode) {
            guard model?.control != false else { return }
            apply(dh_pointer_toggle_lock(&pointer))
            return
        }
        if pointer.locked, isEscape(event.keyCode) {
            apply(dh_pointer_escape(&pointer))
            return
        }
        guard !event.isARepeat else { return }
        sendKey(event.keyCode, down: true)
    }

    override func keyUp(with event: NSEvent) {
        guard !isLockToggle(event.keyCode) else { return }
        sendKey(event.keyCode, down: false)
    }

    override func performKeyEquivalent(with event: NSEvent) -> Bool {
        guard window?.firstResponder === self, model?.control != false else { return false }
        keyDown(with: event)
        return true
    }

    private func isLockToggle(_ macKeyCode: UInt16) -> Bool {
        guard let mapped = DeskhubClient.mapKey(Int32(macKeyCode)) else { return false }
        return dh_is_lock_toggle_vk(mapped.vk)
    }

    private func isEscape(_ macKeyCode: UInt16) -> Bool {
        guard let mapped = DeskhubClient.mapKey(Int32(macKeyCode)) else { return false }
        return dh_is_escape_vk(mapped.vk)
    }

    override func flagsChanged(with event: NSEvent) {
        let flags = event.modifierFlags
        let mask = maskFor(keyCode: event.keyCode)
        if mask.isEmpty {
            lastFlags = flags
            return
        }
        let nowDown = flags.contains(mask)
        let wasDown = lastFlags.contains(mask)
        lastFlags = flags
        guard nowDown != wasDown else { return }
        sendKey(event.keyCode, down: nowDown)
    }

    private func maskFor(keyCode: UInt16) -> NSEvent.ModifierFlags {
        guard let mapped = DeskhubClient.mapKey(Int32(keyCode)) else { return [] }
        switch dh_modifier_class(mapped.vk) {
        case DHModifierShift: return .shift
        case DHModifierControl: return .control
        case DHModifierOption: return .option
        case DHModifierCommand: return .command
        case DHModifierCapsLock: return .capsLock
        default: return []
        }
    }

    private func sendKey(_ macKeyCode: UInt16, down: Bool) {
        guard let mapped = DeskhubClient.mapKey(Int32(macKeyCode)) else { return }
        model?.key(vk: mapped.vk, scan: mapped.scan, down: down)
    }

    override func mouseMoved(with event: NSEvent) { handleMove(event) }
    override func mouseDragged(with event: NSEvent) { handleMove(event) }
    override func rightMouseDragged(with event: NSEvent) { handleMove(event) }
    override func otherMouseDragged(with event: NSEvent) { handleMove(event) }

    private func handleMove(_ event: NSEvent) {
        guard let model else { return }
        if mouseLocked {
            let dx = Int32(event.deltaX.rounded())
            let dy = Int32(event.deltaY.rounded())
            if dx != 0 || dy != 0 { model.mouseMoveRel(dx: dx, dy: dy) }
            return
        }
        let point = convert(event.locationInWindow, from: nil)
        if let norm = normalize(point) { model.mouseMove(nx: norm.nx, ny: norm.ny) }
    }

    override func mouseDown(with _: NSEvent) { button(.left, down: true) }
    override func mouseUp(with _: NSEvent) { button(.left, down: false) }
    override func rightMouseDown(with _: NSEvent) { button(.right, down: true) }
    override func rightMouseUp(with _: NSEvent) { button(.right, down: false) }
    override func otherMouseDown(with event: NSEvent) { button(otherButton(event), down: true) }
    override func otherMouseUp(with event: NSEvent) { button(otherButton(event), down: false) }

    private func otherButton(_ event: NSEvent) -> MouseButton {
        switch event.buttonNumber {
        case 3: .x1
        case 4: .x2
        default: .middle
        }
    }

    private func button(_ btn: MouseButton, down: Bool) {
        if down { window?.makeFirstResponder(self) }
        model?.mouseButton(btn, down: down)
    }

    override func scrollWheel(with event: NSEvent) {
        if event.hasPreciseScrollingDeltas {
            let notches = dh_take_scroll_notches(Double(event.scrollingDeltaY), &scrollCarry)
            if notches != 0 { model?.mouseWheelNotches(notches) }
            return
        }
        let notches = dh_scroll_notches_from_lines(Double(event.scrollingDeltaY))
        if notches != 0 { model?.mouseWheelNotches(notches) }
    }

    override func resignFirstResponder() -> Bool {
        apply(dh_pointer_focus_lost(&pointer))
        return true
    }
}

struct RemoteView: NSViewRepresentable {
    let model: StreamModel
    let videoSize: CGSize
    var mouseLocked: Bool = false
    let onLayerReady: (AVSampleBufferDisplayLayer?) -> Void
    let onLockChanged: (Bool) -> Void

    func makeNSView(context _: Context) -> RemoteVideoView {
        let view = RemoteVideoView()
        view.model = model
        view.videoSize = videoSize
        view.onLockChanged = onLockChanged
        onLayerReady(view.displayLayer)
        DispatchQueue.main.async { view.window?.makeFirstResponder(view) }
        return view
    }

    func updateNSView(_ nsView: RemoteVideoView, context _: Context) {
        nsView.videoSize = videoSize
        nsView.setMouseLocked(mouseLocked, notify: false)
    }

    static func dismantleNSView(_ nsView: RemoteVideoView, coordinator _: ()) {
        nsView.setMouseLocked(false)
        nsView.model?.setLayer(nil)
        nsView.model = nil
    }
}
