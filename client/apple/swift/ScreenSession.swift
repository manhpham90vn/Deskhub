import AVFoundation

private final class HandlerBox {
    let handlers: ScreenSessionHandlers

    init(_ handlers: ScreenSessionHandlers) {
        self.handlers = handlers
    }

    static func unwrap(_ user: UnsafeMutableRawPointer?) -> ScreenSessionHandlers? {
        guard let user else { return nil }
        return Unmanaged<HandlerBox>.fromOpaque(user).takeUnretainedValue().handlers
    }
}

final class ScreenSession: @unchecked Sendable {
    private let handle: OpaquePointer
    private let handlerBox: UnsafeMutableRawPointer

    private init(handle: OpaquePointer, handlerBox: UnsafeMutableRawPointer) {
        self.handle = handle
        self.handlerBox = handlerBox
    }

    static func start(
        address: String, sourceId: UInt8, passcode: String, handlers: ScreenSessionHandlers
    ) -> ScreenSession? {
        let box = Unmanaged.passRetained(HandlerBox(handlers)).toOpaque()

        var callbacks = DHScreenCallbacks()
        callbacks.user = box
        callbacks.onStatus = { line, user in
            HandlerBox.unwrap(user)?.onStatus(line.map { String(cString: $0) } ?? "")
        }
        callbacks.onSize = { width, height, user in
            HandlerBox.unwrap(user)?.onSize(width, height)
        }
        callbacks.onClosed = { reason, user in
            HandlerBox.unwrap(user)?.onClosed(reason.map { String(cString: $0) } ?? "")
        }
        callbacks.onTrustAsked = { verdict, fingerprint, user in
            HandlerBox.unwrap(user)?.onTrustAsked(
                verdict, fingerprint.map { String(cString: $0) } ?? ""
            )
        }

        guard let handle = dh_screen_start(address, sourceId, nil, &callbacks, passcode) else {
            Unmanaged<HandlerBox>.fromOpaque(box).release()
            return nil
        }
        return ScreenSession(handle: handle, handlerBox: box)
    }

    func stop() {
        dh_screen_stop(handle)
        Unmanaged<HandlerBox>.fromOpaque(handlerBox).release()
    }

    func acceptKey() {
        dh_screen_accept_key(handle)
    }

    func rejectKey() {
        dh_screen_reject_key(handle)
    }

    func setLayer(_ layer: AVSampleBufferDisplayLayer?) {
        let ptr = layer.map { Unmanaged.passUnretained($0).toOpaque() }
        dh_screen_set_layer(handle, ptr)
    }

    func key(vk: Int32, scan: Int32, down: Bool) {
        dh_screen_key(handle, vk, scan, down)
    }

    func hotkey(_ hotkey: Hotkey) {
        dh_screen_hotkey(handle, hotkey.vk, hotkey.scan, hotkey.modVk, hotkey.modScan)
    }

    func charTap(_ codepoint: UInt32) {
        dh_screen_char_tap(handle, codepoint)
    }

    func releaseAllInput() {
        dh_screen_release_all_input(handle)
    }

    func mouseMove(nx: Int32, ny: Int32) {
        dh_screen_mouse_move(handle, nx, ny)
    }

    func mouseMoveRel(dx: Int32, dy: Int32) {
        dh_screen_mouse_move_rel(handle, dx, dy)
    }

    func mouseButton(_ button: MouseButton, down: Bool) {
        dh_screen_mouse_button(handle, button.rawValue, down)
    }

    func mouseWheelNotches(_ notches: Int32) {
        dh_screen_mouse_wheel_notches(handle, notches)
    }

    struct Snapshot {
        let phase: Phase
        let statusLine: String
        let endReason: String
        let videoWidth: UInt32
        let videoHeight: UInt32
    }

    func snapshot() -> Snapshot {
        var raw = DHScreenState()
        dh_screen_snapshot(handle, &raw)
        return Snapshot(
            phase: Phase(rawValue: Int(raw.phase.rawValue)) ?? .idle,
            statusLine: DeskhubClient.cString(raw.statusLine),
            endReason: DeskhubClient.cString(raw.endReason),
            videoWidth: raw.videoWidth,
            videoHeight: raw.videoHeight
        )
    }

    func phase() -> Phase {
        Phase(rawValue: Int(dh_screen_phase(handle).rawValue)) ?? .idle
    }

    func offerClipboard(_ text: String) {
        guard !text.isEmpty else { return }
        dh_screen_clip_offer(handle, text)
    }

    func takeClipboard() -> String? {
        let text = DeskhubClient.buffered(33000) { dh_screen_clip_take(handle, $0, $1) }
        return text.isEmpty ? nil : text
    }
}
