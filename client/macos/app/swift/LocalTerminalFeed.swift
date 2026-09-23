import Foundation

@MainActor
final class LocalTerminalFeed: TerminalFeed {
    private let termId: UInt32
    private var closed = false

    init(termId: UInt32) {
        self.termId = termId
    }

    private var alive: Bool {
        !closed && dh_share_local_shell_alive(termId)
    }

    var state: Int32 {
        alive ? TerminalModel.live : TerminalModel.ended
    }

    var message: String {
        DeskhubClient.string(alive ? DHStrTerminalAttachedHere : DHStrTerminalClosed)
    }

    var trustVerdict: Int32 { 0 }
    var fingerprint: String { "" }

    func answerTrust(_: Bool) {}

    func grid(
        scrollOffset: UInt32, into cells: UnsafeMutablePointer<DHTermCell>?, capacity: UInt32,
        info: inout DHTermGrid
    ) -> Bool {
        dh_share_local_grid(termId, scrollOffset, cells, capacity, &info)
    }

    func sendKey(_ key: Int32, codepoint: UInt32, shift: Bool, alt: Bool, ctrl: Bool) {
        dh_share_local_send_key(termId, key, codepoint, shift, alt, ctrl)
    }

    func resize(cols: UInt16, rows: UInt16) {
        dh_share_local_resize(termId, cols, rows)
    }

    func stop() {
        guard !closed else { return }
        closed = true
        dh_share_close_local_shell(termId)
    }
}
