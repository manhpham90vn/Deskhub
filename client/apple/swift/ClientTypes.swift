import Foundation

nonisolated enum Phase: Int, Sendable {
    case idle = 0
    case connecting = 1
    case streaming = 2
    case ended = 3
    case deciding = 4
    case reattaching = 5
}

nonisolated enum MouseButton: Int32, Sendable {
    case left = 1
    case right = 2
    case middle = 3
    case x1 = 4
    case x2 = 5
}

struct Source: Identifiable, Sendable, Hashable, Codable {
    let id: UInt8
    let name: String
    let displayName: String
    let sizeLabel: String
    let pickerLabel: String
}

struct HostCaps: Sendable, Hashable, Codable {
    var acceptsInput = false
    var terminal = false
    var files = false
}

struct HostQuery: Sendable {
    var sources: [Source] = []
    var caps = HostCaps()
}

struct TransferState: Sendable, Equatable {
    var active = false
    var done = false
    var failed = false
    var fileIndex: UInt16 = 0
    var fileCount: UInt16 = 0
    var bytes: UInt64 = 0
    var total: UInt64 = 0
    var name = ""
    var message = ""

    var idle: Bool { !active && !done && !failed }

    var fraction: Double {
        guard total > 0 else { return active ? 0 : 1 }
        return min(1, Double(bytes) / Double(total))
    }

    var step: String {
        guard fileCount > 0 else { return "" }
        return "\(min(Int(fileIndex) + 1, Int(fileCount)))/\(fileCount)"
    }
}

struct ScreenSessionHandlers: Sendable {
    var onStatus: @Sendable (String) -> Void = { _ in }
    var onSize: @Sendable (UInt32, UInt32) -> Void = { _, _ in }
    var onClosed: @Sendable (String) -> Void = { _ in }
    var onTrustAsked: @Sendable (Int32, String) -> Void = { _, _ in }
}
