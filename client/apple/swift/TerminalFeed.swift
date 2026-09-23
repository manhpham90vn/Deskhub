import Foundation

nonisolated enum TermKeyCode {
    static let char: Int32 = 0
    static let enter: Int32 = 1
    static let backspace: Int32 = 2
    static let tab: Int32 = 3
    static let escape: Int32 = 4
    static let up: Int32 = 5
    static let down: Int32 = 6
    static let right: Int32 = 7
    static let left: Int32 = 8
    static let home: Int32 = 9
    static let end: Int32 = 10
    static let pageUp: Int32 = 11
    static let pageDown: Int32 = 12
    static let delete: Int32 = 14
    static let f1: Int32 = 15
}

struct TerminalGridSnapshot {
    var rows = 0
    var cols = 0
    var cursorRow = 0
    var cursorCol = 0
    var cursorVisible = false
    var scrollbackRows = 0
    var scrollOffset = 0
    var cells: [DHTermCell] = []

    func cell(_ row: Int, _ col: Int) -> DHTermCell {
        cells[row * cols + col]
    }
}

struct ShellRow: Identifiable, Equatable {
    var id: UInt32
    var line: String
    var resumable: Bool
    var closable: Bool
}

@MainActor protocol TerminalFeed {
    var state: Int32 { get }
    var message: String { get }
    var trustVerdict: Int32 { get }
    var fingerprint: String { get }
    var shells: [ShellRow] { get }
    var shellsKnown: Bool { get }

    func answerTrust(_ accept: Bool)
    func grid(
        scrollOffset: UInt32, into cells: UnsafeMutablePointer<DHTermCell>?, capacity: UInt32,
        info: inout DHTermGrid
    ) -> Bool
    func sendKey(_ key: Int32, codepoint: UInt32, shift: Bool, alt: Bool, ctrl: Bool)
    func resize(cols: UInt16, rows: UInt16)
    func resumeShell(_ termId: UInt32)
    func closeShell(_ termId: UInt32)
    func openFreshShell()
    func stop()
}

extension TerminalFeed {
    var shells: [ShellRow] { [] }
    var shellsKnown: Bool { false }
    func resumeShell(_: UInt32) {}
    func closeShell(_: UInt32) {}
    func openFreshShell() {}
}
