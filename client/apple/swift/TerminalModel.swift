import Foundation
import Observation

@MainActor
final class RemoteTerminalFeed: TerminalFeed {
    private var handle: OpaquePointer?

    init?(address: String, passcode: String, cols: UInt16, rows: UInt16, picker: Bool = true) {
        var callbacks = DHTermCallbacks()
        callbacks.onTrustAsked = { _, _, _ in }
        let opened = picker
            ? dh_term_open_deferred(address, passcode, cols, rows, &callbacks)
            : dh_term_open(address, passcode, cols, rows, &callbacks)
        guard let opened else { return nil }
        handle = opened
    }

    var shellsKnown: Bool {
        guard let handle else { return false }
        return dh_term_sessions_known(handle)
    }

    var shells: [ShellRow] {
        guard let handle else { return [] }
        var rows: [ShellRow] = []
        var info = DHTermSessionInfo()
        for index in 0 ..< dh_term_session_count(handle) {
            guard dh_term_session_info(handle, index, &info) else { continue }
            rows.append(
                ShellRow(
                    id: info.termId,
                    line: DeskhubClient.text(of: &info.line),
                    resumable: info.resumable,
                    closable: info.closable
                )
            )
        }
        return rows
    }

    func resumeShell(_ termId: UInt32) {
        guard let handle else { return }
        dh_term_resume(handle, termId)
    }

    func closeShell(_ termId: UInt32) {
        guard let handle else { return }
        dh_term_close_session(handle, termId)
    }

    func openFreshShell() {
        guard let handle else { return }
        dh_term_open_new(handle)
    }

    var state: Int32 {
        guard let handle else { return 0 }
        return dh_term_state(handle)
    }

    var message: String {
        guard let handle else { return "" }
        return DeskhubClient.buffered(512) { dh_term_message(handle, $0, $1) }
    }

    var trustVerdict: Int32 {
        guard let handle else { return 0 }
        return dh_term_verdict(handle)
    }

    var fingerprint: String {
        guard let handle else { return "" }
        return DeskhubClient.buffered(128) { dh_term_fingerprint(handle, $0, $1) }
    }

    func answerTrust(_ accept: Bool) {
        guard let handle else { return }
        if accept {
            dh_term_accept_key(handle)
        } else {
            dh_term_reject_key(handle)
        }
    }

    func grid(
        scrollOffset: UInt32, into cells: UnsafeMutablePointer<DHTermCell>?, capacity: UInt32,
        info: inout DHTermGrid
    ) -> Bool {
        guard let handle else { return false }
        return dh_term_grid(handle, scrollOffset, cells, capacity, &info)
    }

    func sendKey(_ key: Int32, codepoint: UInt32, shift: Bool, alt: Bool, ctrl: Bool) {
        guard let handle else { return }
        dh_term_send_key(handle, key, codepoint, shift, alt, ctrl)
    }

    func resize(cols: UInt16, rows: UInt16) {
        guard let handle else { return }
        dh_term_resize(handle, cols, rows)
    }

    func stop() {
        guard let handle else { return }
        dh_term_stop(handle)
        self.handle = nil
    }
}

@MainActor @Observable
final class TerminalModel {
    static let deciding: Int32 = 2
    static let live: Int32 = 4
    static let ended: Int32 = 8

    private var feed: (any TerminalFeed)?
    private var pollTimer: Timer?
    private var lastRevision = UInt64.max
    private var lastOffset = -1
    private var lastScrollbackRows = 0

    var state: Int32 = 0
    var message = ""
    var grid = TerminalGridSnapshot()
    var shells: [ShellRow] = []
    var showingPicker = false
    private var pickerSettled = false
    var askingTrust = false
    var trustChanged = false
    var trustFingerprint = ""
    var latchCtrl = false
    var latchAlt = false
    private(set) var scrollOffset = 0

    func open(address: String, passcode: String, cols: UInt16 = 100, rows: UInt16 = 30) -> Bool {
        stop()
        guard let opened = RemoteTerminalFeed(
            address: address, passcode: passcode, cols: cols, rows: rows
        ) else {
            return false
        }
        attach(opened)
        return true
    }

    func resumeShell(_ termId: UInt32) {
        showingPicker = false
        feed?.resumeShell(termId)
    }

    func closeShell(_ termId: UInt32) {
        feed?.closeShell(termId)
    }

    func openFreshShell() {
        showingPicker = false
        feed?.openFreshShell()
    }

    private func refreshShells() {
        guard let feed, showingPicker || !pickerSettled else { return }
        let rows = feed.shells
        if rows != shells { shells = rows }
        guard !pickerSettled, feed.shellsKnown else { return }
        pickerSettled = true
        if rows.isEmpty {
            feed.openFreshShell()
        } else {
            showingPicker = true
        }
    }

    func attach(_ source: any TerminalFeed) {
        stop()
        feed = source
        lastRevision = .max
        lastOffset = -1
        lastScrollbackRows = 0
        scrollOffset = 0
        shells = []
        showingPicker = false
        pickerSettled = false
        startPolling()
    }

    func scrollBy(rows: Int) {
        guard rows != 0 else { return }
        let next = min(max(scrollOffset + rows, 0), grid.scrollbackRows)
        guard next != scrollOffset else { return }
        scrollOffset = next
        poll()
    }

    func scrollToBottom() {
        guard scrollOffset != 0 else { return }
        scrollOffset = 0
        poll()
    }

    func stop() {
        pollTimer?.invalidate()
        pollTimer = nil
        feed?.stop()
        feed = nil
        state = 0
        shells = []
        showingPicker = false
        pickerSettled = false
    }

    func answerTrust(_ accept: Bool) {
        askingTrust = false
        feed?.answerTrust(accept)
    }

    func typeChar(_ codepoint: UInt32) {
        if latchCtrl || latchAlt {
            sendKey(TermKeyCode.char, codepoint: codepoint, alt: latchAlt, ctrl: latchCtrl)
            latchCtrl = false
            latchAlt = false
        } else if codepoint == 10 || codepoint == 13 {
            sendKey(TermKeyCode.enter)
        } else {
            sendKey(TermKeyCode.char, codepoint: codepoint)
        }
    }

    func typeSpecial(_ key: Int32) {
        sendKey(key, alt: latchAlt, ctrl: latchCtrl)
        latchCtrl = false
        latchAlt = false
    }

    func sendKey(
        _ key: Int32, codepoint: UInt32 = 0, shift: Bool = false, alt: Bool = false,
        ctrl: Bool = false
    ) {
        scrollToBottom()
        feed?.sendKey(key, codepoint: codepoint, shift: shift, alt: alt, ctrl: ctrl)
    }

    func resize(cols: Int, rows: Int) {
        guard cols > 0, rows > 0 else { return }
        feed?.resize(cols: UInt16(clamping: cols), rows: UInt16(clamping: rows))
    }

    private func startPolling() {
        pollTimer = Timer.scheduledTimer(
            withTimeInterval: 0.033, repeats: true
        ) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
    }

    private func poll() {
        guard let feed else { return }
        state = feed.state
        message = feed.message
        refreshShells()

        if state == TerminalModel.deciding, !askingTrust {
            trustChanged = feed.trustVerdict == 2
            trustFingerprint = feed.fingerprint
            askingTrust = true
        }

        var info = DHTermGrid()
        _ = feed.grid(scrollOffset: UInt32(scrollOffset), into: nil, capacity: 0, info: &info)

        let arrived = Int(info.scrollbackRows) - lastScrollbackRows
        lastScrollbackRows = Int(info.scrollbackRows)
        if scrollOffset > 0, arrived > 0 {
            scrollOffset += arrived
        }

        guard info.revision != lastRevision || scrollOffset != lastOffset else { return }
        let needed = Int(info.rows) * Int(info.cols)
        var cells = [DHTermCell](repeating: DHTermCell(), count: max(needed, 1))
        let filled = cells.withUnsafeMutableBufferPointer { ptr in
            feed.grid(
                scrollOffset: UInt32(scrollOffset), into: ptr.baseAddress,
                capacity: UInt32(needed), info: &info
            )
        }
        guard filled || needed == 0 else { return }
        lastRevision = info.revision
        scrollOffset = Int(info.scrollOffset)
        lastOffset = scrollOffset
        grid = TerminalGridSnapshot(
            rows: Int(info.rows),
            cols: Int(info.cols),
            cursorRow: Int(info.cursorRow),
            cursorCol: Int(info.cursorCol),
            cursorVisible: info.cursorVisible,
            scrollbackRows: Int(info.scrollbackRows),
            scrollOffset: scrollOffset,
            cells: Array(cells.prefix(needed))
        )
    }
}
