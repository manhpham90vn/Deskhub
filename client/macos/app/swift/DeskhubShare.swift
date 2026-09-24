import Foundation

struct ShareSource: Identifiable, Hashable, Sendable {
    let rawId: UInt32
    let width: UInt32
    let height: UInt32
    let name: String

    var id: UInt32 { rawId }
}

struct ShareOptions: Sendable {
    let fps: UInt32
    let bitrateMbps: UInt32
    let maxDim: UInt32
    let port: UInt16
    let allowInput: Bool
    let passcode: String
    var terminal: Bool = false
    var files: Bool = false
}

struct HostRow: Identifiable, Hashable, Sendable {
    let viewer: Bool
    let terminal: Bool
    let files: Bool
    let sourceId: UInt8
    let termId: UInt32
    let shellState: UInt8
    let online: Bool
    let viewerAddr: String
    let source: String
    let size: String
    let viewers: String
    let client: String
    let capture: String
    let send: String
    let mbps: String
    let rtt: String

    var id: String { "\(sourceId)/\(termId)/\(viewerAddr)" }

    var attachedLocally: Bool {
        terminal && viewer && shellState == UInt8(DHShellLocal.rawValue)
    }

    var canAttachLocally: Bool {
        terminal && viewer && shellState != UInt8(DHShellLocal.rawValue)
    }
}

struct QualityPreset: Identifiable, Sendable {
    let label: String
    let maxDim: Int

    var id: Int { maxDim }
}

nonisolated enum DeskhubShare {
    static let qualityPresets: [QualityPreset] =
        DeskhubClient.ffiList(
            8, DHQualityPreset(),
            { dh_share_quality_presets($0, $1) },
            { raw in QualityPreset(label: DeskhubClient.cString(raw.label), maxDim: Int(raw.maxDim)) }
        )

    static var hasScreenRecording: Bool { dh_has_screen_recording() }
    static var hasAccessibility: Bool { dh_has_accessibility() }

    static func requestScreenRecording() -> Bool { dh_request_screen_recording() }
    static func requestAccessibility() -> Bool { dh_request_accessibility() }

    static func openScreenRecordingSettings() { dh_open_screen_recording_settings() }
    static func openAccessibilitySettings() { dh_open_accessibility_settings() }

    static func listShareSources() -> [ShareSource] {
        DeskhubClient.ffiList(
            128, DHShareSource(),
            { dh_share_list_sources($0, $1) },
            { info in
                ShareSource(
                    rawId: info.id,
                    width: info.width,
                    height: info.height,
                    name: cString(info.name)
                )
            }
        )
    }

    @discardableResult
    static func start(sources: [ShareSource], options: ShareOptions) -> Bool {
        var raw = sources.map(toRaw)
        return raw.withUnsafeMutableBufferPointer { ptr in
            dh_share_start(
                ptr.baseAddress, Int32(ptr.count), options.fps, options.bitrateMbps,
                options.maxDim, options.port, options.allowInput, options.passcode,
                options.terminal, options.files
            )
        }
    }

    static func stop() { dh_share_stop() }
    static var isRunning: Bool { dh_share_running() }

    static func stopSource(_ sourceId: UInt8) { dh_share_stop_source(sourceId) }

    static func kickViewer(_ sourceId: UInt8, address: String) {
        dh_share_kick_viewer(sourceId, address)
    }

    static func attachShell(_ termId: UInt32) -> Bool { dh_share_attach_shell(termId) }

    static func stopFiles() { dh_share_stop_files() }

    static func openFilesFolder() { dh_share_open_files_folder() }

    static var filesFolder: String {
        DeskhubClient.buffered(1024) { dh_share_files_folder($0, $1) }
    }

    static func setFilesFolder(_ path: String) { dh_set_transfer_dir(path) }

    static var maxSources: Int { Int(dh_max_sources()) }

    static func hostRows() -> [HostRow] {
        DeskhubClient.ffiList(
            64, DHHostRow(),
            { dh_share_rows($0, $1) },
            { row in
                HostRow(
                    viewer: row.viewer,
                    terminal: row.terminal,
                    files: row.files,
                    sourceId: row.sourceId,
                    termId: row.termId,
                    shellState: row.shellState,
                    online: row.online,
                    viewerAddr: cString(row.viewerAddr),
                    source: cString(row.source),
                    size: cString(row.size),
                    viewers: cString(row.viewers),
                    client: cString(row.client),
                    capture: cString(row.capture),
                    send: cString(row.send),
                    mbps: cString(row.mbps),
                    rtt: cString(row.rtt)
                )
            }
        )
    }

    static var lastError: String { String(cString: dh_share_last_error()) }

    private static func toRaw(_ source: ShareSource) -> DHShareSource {
        var raw = DHShareSource()
        raw.id = source.rawId
        raw.width = source.width
        raw.height = source.height
        withUnsafeMutableBytes(of: &raw.name) { dst in
            guard let base = dst.baseAddress else { return }
            let bytes = Array(source.name.utf8.prefix(dst.count - 1))
            base.copyMemory(from: bytes, byteCount: bytes.count)
            base.advanced(by: bytes.count).assumingMemoryBound(to: CChar.self).pointee = 0
        }
        return raw
    }

    private static func cString(_ tuple: some Any) -> String {
        DeskhubClient.cString(tuple)
    }
}
