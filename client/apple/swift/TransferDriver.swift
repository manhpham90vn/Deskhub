import Foundation

struct TransferHistoryRow: Identifiable, Hashable {
    let id: UUID
    var name: String
    var bytes: UInt64
    var ok: Bool
    var detail: String

    init(name: String, bytes: UInt64, ok: Bool, detail: String) {
        id = UUID()
        self.name = name
        self.bytes = bytes
        self.ok = ok
        self.detail = detail
    }
}

@MainActor
protocol TransferDriver: AnyObject, Observable {
    var chosenFiles: [URL] { get set }
    var transfer: TransferState { get }
    var transferError: String { get set }
    var history: [TransferHistoryRow] { get }
    var canSend: Bool { get }

    func sendChosenFiles()
    func cancelTransfer()
    func forgetTransfer()
}

extension TransferDriver {
    var canSend: Bool { !chosenFiles.isEmpty && !transfer.active }
}

@MainActor
func transferHistoryRows(for files: [URL], outcome: TransferState) -> [TransferHistoryRow] {
    files.enumerated().map { index, url in
        let sent = outcome.done || index < Int(outcome.fileIndex)
        return TransferHistoryRow(
            name: url.lastPathComponent,
            bytes: UInt64((try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize) ?? 0),
            ok: sent,
            detail: sent ? "" : outcome.message
        )
    }
}
