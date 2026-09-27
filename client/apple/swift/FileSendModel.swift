import Foundation
import Observation

@MainActor @Observable
final class FileSendModel: TransferDriver {
    var chosenFiles: [URL] = []
    var transfer = TransferState()
    var transferError = ""
    var history: [TransferHistoryRow] = []
    @ObservationIgnored private var settled = true

    var address = ""
    var passcode = ""
    var deviceName = ""

    @ObservationIgnored private nonisolated(unsafe) var handle: OpaquePointer?
    private var poll: Timer?

    var canSend: Bool {
        !chosenFiles.isEmpty && !transfer.active && !address.isEmpty
    }

    deinit {
        if let handle { dh_send_stop(handle) }
    }

    func sendChosenFiles() {
        guard !chosenFiles.isEmpty, !transfer.active else { return }
        transferError = ""

        let paths = chosenFiles.map(\.path)
        var problem = [CChar](repeating: 0, count: 512)
        let ok = withCStrings(paths) { ptr, count in
            dh_send_check(ptr, count, &problem, Int32(problem.count)) != 0
        }
        guard ok else {
            transferError = String(cString: problem)
            return
        }

        release()
        handle = withCStrings(paths) { ptr, count in
            dh_send_start(address, passcode, deviceName, ptr, count)
        }
        guard handle != nil else {
            transferError = DeskhubClient.couldNotConnect(address)
            return
        }
        settled = false
        transfer = TransferState(active: true)
        startPolling()
    }

    func cancelTransfer() {
        guard let handle else { return }
        dh_send_cancel(handle)
    }

    func forgetTransfer() {
        stopPolling()
        release()
        settleTransfer()
        chosenFiles = []
        transferError = ""
        transfer = TransferState()
    }

    private func settleTransfer() {
        guard transfer.done || transfer.failed, !settled else { return }
        settled = true
        history.append(contentsOf: transferHistoryRows(for: chosenFiles, outcome: transfer))
        chosenFiles = []
    }

    private func release() {
        if let handle { dh_send_stop(handle) }
        handle = nil
    }

    private func startPolling() {
        stopPolling()
        poll = Timer.scheduledTimer(withTimeInterval: 0.2, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }

    private func stopPolling() {
        poll?.invalidate()
        poll = nil
    }

    private func refresh() {
        guard let handle else { return }
        var raw = DHSendProgress()
        dh_send_snapshot(handle, &raw)
        transfer = TransferState(
            active: raw.state == DHSendConnecting.rawValue || raw.state == DHSendSending.rawValue,
            done: raw.state == DHSendDone.rawValue,
            failed: raw.state == DHSendFailed.rawValue
                || raw.state == DHSendRefused.rawValue
                || raw.state == DHSendKeyChanged.rawValue,
            fileIndex: raw.fileIndex,
            fileCount: raw.fileCount,
            bytes: raw.bytes,
            total: raw.total,
            name: DeskhubClient.cString(raw.name),
            message: DeskhubClient.cString(raw.message)
        )
        if raw.finished {
            stopPolling()
            settleTransfer()
        }
    }

    private func withCStrings<T>(
        _ values: [String], _ body: (UnsafePointer<UnsafePointer<CChar>?>?, Int32) -> T
    ) -> T {
        let copies = values.map { strdup($0) }
        defer { copies.forEach { free($0) } }
        var pointers: [UnsafePointer<CChar>?] = copies.map { UnsafePointer($0) }
        return pointers.withUnsafeMutableBufferPointer { buf in
            body(buf.baseAddress, Int32(buf.count))
        }
    }
}
