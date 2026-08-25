import Foundation

nonisolated enum DeskhubClient {
    static func string(_ id: DHStringId) -> String {
        String(cString: dh_string(id))
    }

    static func connectingTo(_ address: String) -> String {
        buffered(320) { dh_connecting_to(address, $0, $1) }
    }

    static func couldNotConnect(_ address: String) -> String {
        buffered(320) { dh_could_not_connect(address, $0, $1) }
    }

    static func sourceQueryFailed(_ address: String) -> String {
        buffered(320) { dh_source_query_failed(address, $0, $1) }
    }

    static func hostTitle(_ address: String, width: UInt32, height: UInt32) -> String {
        buffered(320) { dh_host_title(address, width, height, $0, $1) }
    }

    static func zoomLabel(_ zoom: Double) -> String {
        buffered(32) { dh_zoom_label(zoom, $0, $1) }
    }

    static func isZoomed(_ zoom: Double) -> Bool {
        dh_is_zoomed(zoom)
    }

    static func linkQualityText(_ quality: DHLinkQuality) -> String {
        buffered(64) { dh_link_quality_text(quality, $0, $1) }
    }

    static func linkPingText(haveRtt: Bool, rttMs: UInt32) -> String {
        buffered(32) { dh_link_ping_text(haveRtt, rttMs, $0, $1) }
    }

    static func buffered(
        _ capacity: Int, _ fill: (UnsafeMutablePointer<CChar>, Int32) -> Int32
    ) -> String {
        var buf = [CChar](repeating: 0, count: capacity)
        _ = fill(&buf, Int32(capacity))
        return String(cString: buf)
    }

    static func viewerBaseTitle(_ sourceName: String) -> String {
        buffered(320) { dh_viewer_base_title(sourceName, $0, $1) }
    }

    static func pointerSubtitle(locked: Bool, statusLine: String) -> String {
        buffered(320) { dh_pointer_subtitle(DHPointerLock(locked: locked), statusLine, $0, $1) }
    }

    static func viewOnlySubtitle(statusLine: String) -> String {
        buffered(320) { dh_view_only_subtitle(statusLine, $0, $1) }
    }

    static func ffiList<Raw, Item>(
        _ capacity: Int, _ empty: Raw,
        _ fill: (UnsafeMutablePointer<Raw>?, Int32) -> Int32,
        _ transform: (Raw) -> Item
    ) -> [Item] {
        var buf = [Raw](repeating: empty, count: capacity)
        let count = buf.withUnsafeMutableBufferPointer { ptr in
            fill(ptr.baseAddress, Int32(ptr.count))
        }
        guard count > 0 else { return [] }
        return (0 ..< Int(count)).map { transform(buf[$0]) }
    }

    static func cString(_ tuple: some Any) -> String {
        withUnsafeBytes(of: tuple) { rawBuf in
            guard let base = rawBuf.baseAddress else { return "" }
            return String(cString: base.assumingMemoryBound(to: CChar.self))
        }
    }

    static func normalizedAddress(_ raw: String) -> String? {
        let addr = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !addr.isEmpty, dh_parse_address(addr) else { return nil }
        return addr
    }

    static func composeAddress(_ host: String, portText: String) -> String {
        buffered(128) { dh_compose_address(host, portText, $0, $1) }
    }

    static func addressHost(_ address: String) -> String {
        buffered(128) { dh_address_host(address, $0, $1) }
    }

    static func sameDeviceAddr(_ left: String, _ right: String) -> Bool {
        dh_same_device_addr(left, right)
    }

    static func addressPortText(_ address: String) -> String {
        let explicit = dh_address_port(address)
        let port = explicit != 0 ? UInt16(explicit) : DeskhubDiscovery.defaultPort
        return String(port)
    }

    static func connectDecision(_ sources: [Source]) -> (showPicker: Bool, sourceId: UInt8) {
        let infos = sources.map { source -> DHSourceInfo in
            var info = DHSourceInfo()
            info.sourceId = source.id
            return info
        }
        var sourceId: UInt8 = 0
        let showPicker = infos.withUnsafeBufferPointer {
            dh_connect_decision($0.baseAddress, Int32($0.count), &sourceId)
        }
        return (showPicker, sourceId)
    }

    static var maxTransferFiles: Int {
        Int(dh_max_transfer_files())
    }

    static var passcodeDigits: Int {
        Int(dh_passcode_digits())
    }

    static func isValidPasscode(_ passcode: String) -> Bool {
        dh_is_valid_passcode(passcode)
    }

    static func passcodeInput(_ typed: String) -> String {
        String(typed.filter(\.isASCII).filter(\.isNumber).prefix(passcodeDigits))
    }

    static func listSources(address: String, passcode: String) -> HostQuery? {
        var buf = [DHSourceInfo](repeating: DHSourceInfo(), count: Int(dh_max_sources()))
        var caps = DHHostCaps()
        let count = buf.withUnsafeMutableBufferPointer { ptr in
            dh_list_sources(address, ptr.baseAddress, Int32(ptr.count), passcode, &caps)
        }
        guard count >= 0 else { return nil }
        let sources = buf.prefix(Int(count)).map { info in
            Source(
                id: info.sourceId,
                name: cString(info.name),
                displayName: cString(info.displayName),
                sizeLabel: cString(info.sizeLabel),
                pickerLabel: cString(info.pickerLabel)
            )
        }
        return HostQuery(
            sources: sources,
            caps: HostCaps(acceptsInput: caps.acceptsInput, terminal: caps.terminal,
                           files: caps.files)
        )
    }
}
