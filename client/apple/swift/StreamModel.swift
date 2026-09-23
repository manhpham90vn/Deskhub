import AVFoundation
import Foundation
import Observation

@MainActor @Observable
final class StreamModel {
    let address: String
    let passcode: String
    let control: Bool
    private(set) var sourceId: UInt8
    private(set) var sourceName: String

    var phase: Phase = .connecting
    var statusLine = ""
    var endReason = ""
    var videoWidth: UInt32 = 0
    var videoHeight: UInt32 = 0
    var failedToStart = false

    var mouseLocked = false

    var askingTrust = false
    var trustChanged = false
    var trustFingerprint = ""

    private var session: ScreenSession?
    private var layer: AVSampleBufferDisplayLayer?
    private var phaseTimer: Timer?

    var reattaching: Bool { phase == .reattaching }

    init(address: String, passcode: String, sourceId: UInt8, sourceName: String,
         control: Bool = true)
    {
        self.address = address
        self.passcode = passcode
        self.control = control
        self.sourceId = sourceId
        self.sourceName = sourceName
    }

    func start() async {
        let addr = address
        let sid = sourceId
        let code = passcode
        let handlers = makeHandlers()
        let opened = await Task.detached {
            ScreenSession.start(address: addr, sourceId: sid, passcode: code, handlers: handlers)
        }.value
        guard let opened else {
            failedToStart = true
            phase = .ended
            endReason = DeskhubClient.couldNotConnect(address)
            return
        }
        failedToStart = false
        session = opened
        opened.setLayer(layer)
        refresh()
        startPhasePolling()
    }

    func switchSource(to newSourceId: UInt8, name: String) async {
        guard newSourceId != sourceId else { return }
        stopPhasePolling()
        session?.stop()
        session = nil
        sourceId = newSourceId
        sourceName = name
        endReason = ""
        statusLine = ""
        videoWidth = 0
        videoHeight = 0
        phase = .connecting
        await start()
    }

    func disconnect() {
        stopPhasePolling()
        mouseLocked = false
        session?.stop()
        session = nil
        phase = .idle
        statusLine = ""
    }

    func setLayer(_ newLayer: AVSampleBufferDisplayLayer?) {
        layer = newLayer
        session?.setLayer(newLayer)
    }

    func key(vk: Int32, scan: Int32, down: Bool) {
        guard control else { return }
        session?.key(vk: vk, scan: scan, down: down)
    }

    func hotkey(_ hotkey: Hotkey) {
        guard control else { return }
        session?.hotkey(hotkey)
    }

    func charTap(_ codepoint: UInt32) {
        guard control else { return }
        session?.charTap(codepoint)
    }

    func releaseAllInput() {
        session?.releaseAllInput()
    }

    func mouseMove(nx: Int32, ny: Int32) {
        guard control else { return }
        session?.mouseMove(nx: nx, ny: ny)
    }

    func mouseMoveRel(dx: Int32, dy: Int32) {
        guard control else { return }
        session?.mouseMoveRel(dx: dx, dy: dy)
    }

    func mouseButton(_ button: MouseButton, down: Bool) {
        guard control else { return }
        session?.mouseButton(button, down: down)
    }

    func mouseWheelNotches(_ notches: Int32) {
        guard control else { return }
        session?.mouseWheelNotches(notches)
    }

    func answerTrust(_ accept: Bool) {
        askingTrust = false
        if accept {
            session?.acceptKey()
        } else {
            session?.rejectKey()
        }
    }

    func offerClipboard(_ text: String) {
        session?.offerClipboard(text)
    }

    func takeClipboard() -> String? {
        session?.takeClipboard()
    }

    var aspectRatio: Double {
        guard videoWidth > 0, videoHeight > 0 else { return 16.0 / 9.0 }
        return Double(videoWidth) / Double(videoHeight)
    }

    func refresh() {
        guard let session else { return }
        let state = session.snapshot()
        phase = state.phase
        statusLine = state.statusLine
        videoWidth = state.videoWidth
        videoHeight = state.videoHeight
        if phase == .ended {
            endReason = state.endReason
            mouseLocked = false
        }
    }

    private func startPhasePolling() {
        phaseTimer?.invalidate()
        phaseTimer = Timer.scheduledTimer(
            withTimeInterval: 1, repeats: true
        ) { [weak self] _ in
            Task { @MainActor in self?.pollPhase() }
        }
    }

    private func stopPhasePolling() {
        phaseTimer?.invalidate()
        phaseTimer = nil
    }

    private func pollPhase() {
        guard let session, phase != .ended else {
            stopPhasePolling()
            return
        }
        let livePhase = session.phase()
        if livePhase != .ended, livePhase != phase {
            phase = livePhase
        }
    }

    private func makeHandlers() -> ScreenSessionHandlers {
        ScreenSessionHandlers(
            onStatus: { [weak self] line in
                Task { @MainActor in
                    guard let self else { return }
                    self.statusLine = line
                    self.phase = self.session?.phase() ?? self.phase
                }
            },
            onSize: { [weak self] width, height in
                Task { @MainActor in
                    guard let self else { return }
                    self.videoWidth = width
                    self.videoHeight = height
                    self.phase = self.session?.phase() ?? self.phase
                }
            },
            onClosed: { [weak self] reason in
                Task { @MainActor in
                    guard let self else { return }
                    self.stopPhasePolling()
                    self.phase = .ended
                    self.endReason = reason
                    self.mouseLocked = false
                }
            },
            onTrustAsked: { [weak self] verdict, fingerprint in
                Task { @MainActor in
                    guard let self else { return }
                    self.trustChanged = verdict == 2
                    self.trustFingerprint = fingerprint
                    self.askingTrust = true
                }
            }
        )
    }
}
