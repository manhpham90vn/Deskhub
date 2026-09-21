import AppKit
import Foundation
import Observation

@MainActor @Observable
final class SharingModel {
    private static let stored = dh_settings_load()

    var fps = Int(SharingModel.stored.fps)
    var bitrateMbps = Int(SharingModel.stored.bitrateMbps)
    var maxDim = Int(SharingModel.stored.maxDim)
    var port = Int(SharingModel.stored.port)
    var allowInput = SharingModel.stored.allowInput
    var passcode = DeskhubClient.cString(SharingModel.stored.passcode) {
        didSet {
            if passcode.isEmpty || DeskhubClient.isValidPasscode(passcode) {
                lastValidPasscode = passcode
            }
        }
    }

    private var lastValidPasscode = DeskhubClient.cString(SharingModel.stored.passcode)

    var addresses: [LocalAddress] = []

    var isSharing = false
    var isStarting = false
    var startError = ""
    var clampWarning = ""
    var rows: [HostRow] = []
    var shareSources: [ShareSource] = []
    var tickedSources: Set<UInt32> = []
    var shareTerminal = true
    var shareFiles = true
    var filesFolder = DeskhubShare.filesFolder
    let pairing = PairingAskModel()

    var hasScreenRecording = false
    var hasAccessibility = false
    var screenRecordingNeedsRelaunch = false

    var acceptedPasscode: String {
        passcode.isEmpty || DeskhubClient.isValidPasscode(passcode)
            ? passcode : lastValidPasscode
    }

    var clientControl = SharingModel.stored.clientControl
    var bindIp = DeskhubClient.buffered(64) { dh_bind_ip($0, $1) }
    var autoShare = dh_auto_share()
    var autostart = dh_autostart_enabled()
    var startHidden = dh_start_hidden()
    var clipboardSync = dh_clipboard_sync()
    var shareAudio = dh_share_audio()
    var playAudio = dh_play_audio()
    var keepAwake = dh_keep_awake()
    var didAutoShare = false
    var autoShareWaitNote = ""
    private var sharingScreen = false
    private var sharingTerminal = false
    private var sharingFiles = false
    private var lastPasteboardChange = NSPasteboard.general.changeCount

    func applyAutostart() {
        dh_set_autostart(autostart)
        autostart = dh_autostart_enabled()
    }

    var statusLine: String {
        let portNum = UInt16(max(1, min(65535, port)))
        guard isSharing else {
            if !autoShareWaitNote.isEmpty { return autoShareWaitNote }
            return DeskhubClient.buffered(128) { dh_idle_host_status(portNum, $0, $1) }
        }
        var line = DeskhubClient.buffered(320) {
            dh_sharing_status(
                portNum, acceptedPasscode, allowInput, sharingScreen, sharingTerminal,
                sharingFiles, $0, $1
            )
        }
        let bindWarning = DeskhubClient.buffered(256) { dh_share_bind_warning($0, $1) }
        if !bindWarning.isEmpty { line += "\n" + bindWarning }
        return line
    }

    func save() {
        dh_settings_save(
            UInt32(max(1, fps)),
            UInt32(max(1, bitrateMbps)),
            maxDim <= 0 ? 0 : UInt32(maxDim),
            UInt32(max(1, port)),
            allowInput,
            clientControl,
            acceptedPasscode
        )
        dh_set_bind_ip(bindIp)
        dh_set_auto_share(autoShare)
        dh_set_start_hidden(startHidden)
        dh_set_clipboard_sync(clipboardSync)
        dh_set_share_audio(shareAudio)
        dh_set_play_audio(playAudio)
        dh_set_keep_awake(keepAwake)
    }

    private var pollTimer: Timer?

    func refreshPermissions() {
        hasScreenRecording = DeskhubShare.hasScreenRecording
        hasAccessibility = DeskhubShare.hasAccessibility
        if hasScreenRecording {
            screenRecordingNeedsRelaunch = false
        }
    }

    func requestScreenRecording() {
        hasScreenRecording = DeskhubShare.requestScreenRecording()
        screenRecordingNeedsRelaunch = !hasScreenRecording
    }

    func requestAccessibility() {
        hasAccessibility = DeskhubShare.requestAccessibility()
    }

    func loadAddresses() {
        addresses = LocalAddress.all()
    }

    private var sourcesRefresh: Task<Void, Never>?

    func refreshShareSources() async {
        if let running = sourcesRefresh {
            await running.value
            return
        }
        let task = Task { await self.loadShareSources() }
        sourcesRefresh = task
        await task.value
        sourcesRefresh = nil
    }

    func waitForShareSources() async -> Bool {
        let probeMs = dh_auto_share_probe_ms()
        var waitedMs: UInt32 = 0
        while true {
            await refreshShareSources()
            switch dh_auto_share_step(!shareSources.isEmpty, waitedMs) {
            case DHAutoShareShareNow:
                autoShareWaitNote = ""
                return true
            case DHAutoShareGiveUpWaiting:
                autoShareWaitNote = DeskhubClient.string(DHStrNoDisplayFound)
                return false
            default:
                autoShareWaitNote = DeskhubClient.string(DHStrWaitingForDisplays)
                try? await Task.sleep(for: .milliseconds(Int(probeMs)))
                waitedMs += probeMs
            }
        }
    }

    var pickedSources: [ShareSource] {
        shareSources.filter { tickedSources.contains($0.id) }
    }

    func startSharing() async -> Bool {
        guard !isStarting, !isSharing else { return false }
        autoShareWaitNote = ""
        guard passcode.isEmpty || DeskhubClient.isValidPasscode(passcode) else {
            startError = DeskhubClient.string(DHStrPasscodeInvalid)
            return false
        }
        let terminal = shareTerminal
        let files = shareFiles
        if shareSources.isEmpty { await refreshShareSources() }
        guard !shareSources.isEmpty || terminal || files else {
            startError = DeskhubClient.string(DHStrScreenRecordingRequired)
            return false
        }
        guard !pickedSources.isEmpty || terminal || files else {
            startError = DeskhubClient.string(DHStrNoDisplayTicked)
            return false
        }

        let picked = Array(pickedSources.prefix(DeskhubShare.maxSources))
        clampWarning = picked.count < pickedSources.count
            ? DeskhubClient.string(DHStrShareClampWarning) : ""

        isStarting = true
        startError = ""
        save()

        let options = ShareOptions(
            fps: UInt32(max(1, fps)),
            bitrateMbps: UInt32(max(1, bitrateMbps)),
            maxDim: maxDim <= 0 ? UInt32(0) : UInt32(maxDim),
            port: UInt16(max(1, min(65535, port))),
            allowInput: allowInput,
            passcode: acceptedPasscode,
            terminal: terminal,
            files: files
        )

        let ok = await Task.detached {
            DeskhubShare.start(sources: picked, options: options)
        }.value

        isStarting = false
        isSharing = ok
        if ok {
            noteSharingBegan(screen: !picked.isEmpty, terminal: terminal, files: files)
        } else {
            clampWarning = ""
            startError = picked.isEmpty || hasScreenRecording
                ? DeskhubClient.string(DHStrShareStartFailed) + ". " + DeskhubShare.lastError
                : DeskhubClient.string(DHStrScreenRecordingRequired)
        }
        return ok
    }

    private func noteSharingBegan(screen: Bool, terminal: Bool, files: Bool) {
        sharingScreen = screen
        sharingTerminal = terminal
        sharingFiles = files
        filesFolder = DeskhubShare.filesFolder
        startPolling()
    }

    func stopSharing() {
        stopPolling()
        DeskhubShare.stop()
        pairing.clear()
        isSharing = false
        sharingScreen = false
        sharingTerminal = false
        sharingFiles = false
        rows = []
        Task { await refreshShareSources() }
    }

    private func startPolling() {
        stopPolling()
        pollTimer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
        poll()
    }

    private func stopPolling() {
        pollTimer?.invalidate()
        pollTimer = nil
    }

    private func poll() {
        rows = DeskhubShare.hostRows()
        if !DeskhubShare.isRunning {
            stopSharing()
            return
        }
        pairing.drain()
        if clipboardSync { pumpClipboard() }
    }
}

extension SharingModel {
    func runRowAction(_ row: HostRow) {
        guard isSharing else { return }
        if row.terminal {
            if row.viewer {
                dh_share_kick_shell(row.termId)
            } else {
                dh_share_stop_terminal()
                sharingTerminal = false
                if !rows.contains(where: { !$0.terminal }) { stopSharing() }
            }
            return
        }
        if row.files {
            guard !row.viewer else { return }
            DeskhubShare.stopFiles()
            sharingFiles = false
            if !rows.contains(where: { !$0.files }) { stopSharing() }
            return
        }
        if row.viewer {
            DeskhubShare.kickViewer(row.sourceId, address: row.viewerAddr)
        } else {
            DeskhubShare.stopSource(row.sourceId)
        }
    }

    func stopAndAttachShell(_ row: HostRow) -> Bool {
        guard isSharing, row.canAttachLocally else { return false }
        guard DeskhubShare.attachShell(row.termId) else { return false }
        rows = DeskhubShare.hostRows()
        return true
    }

    private func loadShareSources() async {
        guard !isSharing, !isStarting else { return }
        refreshPermissions()
        guard hasScreenRecording else {
            shareSources = []
            tickedSources = []
            return
        }
        let found = await Task.detached { DeskhubShare.listShareSources() }.value
        shareSources = found
        tickedSources = Set(found.map(\.id))
    }

    private func pumpClipboard() {
        let board = NSPasteboard.general
        let remote = DeskhubClient.buffered(33000) { dh_share_clip_take($0, $1) }
        if !remote.isEmpty {
            board.clearContents()
            board.setString(remote, forType: .string)
            lastPasteboardChange = board.changeCount
            return
        }
        guard board.changeCount != lastPasteboardChange else { return }
        lastPasteboardChange = board.changeCount
        if let text = board.string(forType: .string), !text.isEmpty {
            dh_share_clip_offer(text)
        }
    }
}
