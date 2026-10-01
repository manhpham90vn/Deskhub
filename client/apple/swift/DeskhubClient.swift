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

    static func text(of field: inout some Any) -> String {
        withUnsafeBytes(of: &field) { raw in
            guard let base = raw.baseAddress else { return "" }
            return String(cString: base.assumingMemoryBound(to: CChar.self))
        }
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

    static var defaultPort: UInt16 { dh_default_port() }

    static func addressPortText(_ address: String) -> String {
        let explicit = dh_address_port(address)
        let port = explicit != 0 ? UInt16(explicit) : defaultPort
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

    private static let queryFailureCapacity = 512
    private static let hostKeyCapacity = 128
    private static let trustPromptCapacity = 640
    private static let answeredAddressCapacity = 128
    private static let queryStatusCapacity = 320
    private static let pairingInviteCapacity = Int(DH_PAIRING_INVITE_CAP)
    private static let qrModuleCapacity = Int(DH_QR_MAX_SIZE) * Int(DH_QR_MAX_SIZE)
    private static let publicKeyCapacity = 512
    private static let accessRequestCapacity = 16

    static func trustNewHostPrompt(_ address: String, fingerprint: String) -> String {
        buffered(trustPromptCapacity) { dh_trust_new_host_prompt(address, fingerprint, $0, $1) }
    }

    static func listSources(address: String, invite: String) -> HostQueryOutcome {
        var buf = [DHSourceInfo](repeating: DHSourceInfo(), count: Int(dh_max_sources()))
        var caps = DHHostCaps()
        var failure = [CChar](repeating: 0, count: queryFailureCapacity)
        var newHostKey = [CChar](repeating: 0, count: hostKeyCapacity)
        var answered = [CChar](repeating: 0, count: answeredAddressCapacity)
        var failureKind: Int32 = 0
        let count = buf.withUnsafeMutableBufferPointer { ptr in
            dh_list_sources(
                address, invite, ptr.baseAddress, Int32(ptr.count), &caps,
                &failure, Int32(queryFailureCapacity),
                &newHostKey, Int32(hostKeyCapacity),
                &answered, Int32(answeredAddressCapacity),
                &failureKind
            )
        }
        var outcome = HostQueryOutcome(
            failure: String(cString: failure),
            newHostKey: String(cString: newHostKey),
            answeredAddress: String(cString: answered),
            failureKind: failureKind
        )
        guard count >= 0 else { return outcome }
        let sources = buf.prefix(Int(count)).map { info in
            Source(
                id: info.sourceId,
                name: cString(info.name),
                displayName: cString(info.displayName),
                sizeLabel: cString(info.sizeLabel),
                pickerLabel: cString(info.pickerLabel)
            )
        }
        outcome.query = HostQuery(
            sources: sources,
            caps: HostCaps(acceptsInput: caps.acceptsInput, terminal: caps.terminal,
                           files: caps.files)
        )
        return outcome
    }

    static func cancelSourceQuery() {
        dh_list_sources_cancel()
    }

    static func sourceQueryStatus() -> String {
        buffered(queryStatusCapacity) { dh_source_query_status($0, $1) }
    }

    static func isPairingInvite(_ text: String) -> Bool {
        dh_is_pairing_invite(text)
    }

    static func pairingInviteAddress(_ invite: String) -> String {
        buffered(answeredAddressCapacity) { dh_pairing_invite_address(invite, $0, $1) }
    }

    static func pairingInviteNewHostKey(_ invite: String) -> String {
        buffered(hostKeyCapacity) { dh_pairing_invite_new_host_key(invite, $0, $1) }
    }

    static func pairingInvite(port: UInt16, bindIp: String) -> String {
        buffered(pairingInviteCapacity) { dh_pairing_invite(port, bindIp, $0, $1) }
    }

    static var pairingTokenTtlSeconds: Int64 { dh_pairing_token_ttl_seconds() }

    static func qrExpiryLine(_ secondsLeft: Int64) -> String {
        buffered(64) { dh_qr_expiry_line(secondsLeft, $0, $1) }
    }

    static func revokePairingInvite() {
        dh_pairing_revoke()
    }

    static func qrModules(_ text: String) -> [[Bool]] {
        var modules = [UInt8](repeating: 0, count: qrModuleCapacity)
        let size = Int(dh_qr_encode(text, &modules, Int32(qrModuleCapacity)))
        guard size > 0 else { return [] }
        return (0 ..< size).map { row in
            (0 ..< size).map { column in modules[row * size + column] != 0 }
        }
    }

    static func hostPublicKey() -> String {
        buffered(publicKeyCapacity) { dh_host_public_key($0, $1) }
    }

    static func accessRequests() -> [AccessRequestRow] {
        ffiList(
            accessRequestCapacity, DHAccessRequest(),
            { dh_access_requests($0, $1) },
            { raw in
                AccessRequestRow(
                    name: cString(raw.name),
                    address: cString(raw.address),
                    shortKey: cString(raw.shortKey),
                    fingerprint: cString(raw.fingerprint)
                )
            }
        )
    }

    static func approveAccess(_ fingerprint: String) -> Bool {
        dh_access_approve(fingerprint)
    }

    static func denyAccess(_ fingerprint: String) -> Bool {
        dh_access_deny(fingerprint)
    }

    static var accessRequestsGeneration: UInt64 {
        dh_access_requests_generation()
    }

    static func accessRequestNotification(name: String, address: String) -> String {
        buffered(320) { dh_access_request_notification(name, address, $0, $1) }
    }
}
