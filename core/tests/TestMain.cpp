#include "Tests.h"
#include "support/TestSupport.h"

#include <cstdio>

int main() {
    std::printf("=== core self-test (offline: no network, no GPU) ===\n");

    std::printf("--- wire: big-endian field accessors ---\n");
    RunByteOrderTests();

    std::printf("--- wire ---\n");
    RunWireTests();

    std::printf("--- transport: reassembler ---\n");
    RunReassemblerTests();
    RunAudioJitterBufferTests();

    std::printf("--- transport: FEC ---\n");
    RunFecTests();

    std::printf("--- transport: retransmit/NACK ---\n");
    RunRetransmitCacheTests();

    std::printf("--- transport: send pacer ---\n");
    RunPacerTests();
    RunLossGoodputTests();

    std::printf("--- session ---\n");
    RunScreenSessionTests();

    std::printf("--- session: client pump (ingest + keyframes + reporting) ---\n");
    RunScreenClientTests();

    std::printf("--- session: clipboard sync (chunking, dedupe, echo suppression) ---\n");
    RunClipboardSyncTests();

    std::printf("--- session: host feedback policy (bitrate, FEC, quality, NACK) ---\n");
    RunViewerFeedbackTests();

    std::printf("--- session: host router (demux, re-offer, keepalive timing) ---\n");
    RunSourcePipelineTests();

    std::printf("--- session: many viewers per source (fan-out, input priority) ---\n");
    RunViewerTableTests();

    std::printf("--- session: viewer connect flow ---\n");
    RunConnectFlowTests();

    std::printf("--- session: elevated share args + source clamp ---\n");
    RunShareFlowTests();

    std::printf("--- session: open viewer count ---\n");
    RunOpenViewersTests();

    std::printf("--- session: per-source pipeline state defaults ---\n");
    RunSourcePipelineStateTests();

    std::printf("--- session: remote terminal (host sessions, client, stream framing) ---\n");
    RunTerminalSessionTests();

    std::printf("--- transfer: file names the filesystem can be trusted with ---\n");
    RunSafeNameTests();

    std::printf("--- transfer: CRC-32 over chunked files ---\n");
    RunCrc32Tests();

    std::printf("--- transfer: client-to-host file batches (offer, chunks, checksums) ---\n");
    RunFileTransferTests();

    std::printf("--- session: keeping a link alive and getting it back ---\n");
    RunLinkRecoveryTests();

    std::printf("--- session: the link's own ping, loss and quality reading ---\n");
    RunLinkPulseTests();

    std::printf("--- session: passcode attempt throttle ---\n");
    RunAuthThrottleTests();

    std::printf("--- input ---\n");
    RunInputTests();

    std::printf("--- input: held keys/buttons + host-wins gate ---\n");
    RunPressedInputTests();

    std::printf("--- input: client-side queue (taps, chords, delayed release) ---\n");
    RunClientInputQueueTests();

    std::printf("--- input: shared scancode table lookups ---\n");
    RunScancodeTableTests();

    std::printf("--- input: the vk -> set-1 scancode table as a whole ---\n");
    RunSet1ScancodeTests();

    std::printf("--- input: shared on-screen hotkey bar ---\n");
    RunHotkeysTests();

    std::printf("--- input: pointer mapping + shared injector dispatch ---\n");
    RunPointerMapTests();

    std::printf("--- input: viewer pointer lock ---\n");
    RunPointerLockStateTests();

    std::printf("--- input: touch trackpad cursor ---\n");
    RunTrackpadCursorTests();

    std::printf("--- control: bitrate + link stats ---\n");
    RunControlTests();

    std::printf("--- control: stream size negotiation ---\n");
    RunStreamSizeTests();

    std::printf("--- control: per-source frame rate gate ---\n");
    RunFrameGateTests();

    std::printf("--- control: clock offset / one-way latency ---\n");
    RunClockOffsetTests();

    std::printf("--- control: video pacer (arrival jitter -> display cadence) ---\n");
    RunVideoPacerTests();

    std::printf("--- control: quality ladder (fps + resolution vs bandwidth) ---\n");
    RunQualityLadderTests();

    std::printf("--- diag: window counters + log line formatting ---\n");
    RunDiagTests();

    std::printf("--- media: encoder/decoder signature contract ---\n");
    RunMediaContractTests();

    std::printf("--- media: H.264 bit writer (exp-golomb + emulation prevention) ---\n");
    RunBitWriterTests();

    std::printf("--- media: H.264 SPS rewrite (VUI with zero reorder delay) ---\n");
    RunH264SpsTests();

    std::printf("--- media: Annex-B NAL parsing ---\n");
    RunAnnexBTests();

    std::printf("--- media: encoder rate plan (VBV sizing) ---\n");
    RunRatePlanTests();

    std::printf("--- media: H.264 macroblock geometry and levels ---\n");
    RunH264EncodeTests();

    std::printf("--- media: share quality presets ---\n");
    RunQualityPresetTests();

    std::printf("--- media: view fit (letterbox, zoom/pan, pointer mapping) ---\n");
    RunViewFitTests();

    std::printf("--- media: source names and size labels ---\n");
    RunSourceLabelTests();

    std::printf("--- media: viewer window title (status + lock hint) ---\n");
    RunViewerTitleTests();

    std::printf("--- media: latest-wins frame mailbox ---\n");
    RunFrameMailboxTests();

    std::printf("--- media: area-average RGB downscale ---\n");
    RunRgbDownscaleTests();

    std::printf("--- media: PCM ring (drop-oldest, silence padding) ---\n");
    RunPcmRingTests();

    std::printf("--- beacon (pre-session LIST_SOURCES + PING) ---\n");
    RunBeaconTests();

    std::printf("--- net: which addresses a LAN scan should try ---\n");
    RunLanScanTests();

    std::printf("--- net: dotted-quad IPv4 parsing ---\n");
    RunIpv4Tests();

    std::printf("--- net: which address the host binds ---\n");
    RunBindAddressTests();

    std::printf("--- net: which host keys we have decided to trust ---\n");
    RunTrustStoreTests();

    std::printf("--- net: which machines this host has paired with ---\n");
    RunPairedDevicesTests();

    std::printf("--- terminal: the VT escape-sequence parser ---\n");
    RunVtParserTests();

    std::printf("--- terminal: the character grid every client draws ---\n");
    RunScreenTests();

    std::printf("--- terminal: the cell snapshot every window renders from ---\n");
    RunSnapshotTests();

    std::printf("--- terminal: repainting a client that fell behind ---\n");
    RunRepaintTests();

    std::printf("--- terminal: keys and modifiers to bytes ---\n");
    RunKeyEncoderTests();

    std::printf("--- terminal: the colours every client paints with ---\n");
    RunPaletteTests();

    std::printf("--- terminal: keeping the view still while output arrives ---\n");
    RunScrollAnchorTests();

    std::printf("--- ui: shared strings every client shows ---\n");
    RunStringsTests();

    std::printf("--- ui: host table rows (displays, viewers, cells) ---\n");
    RunHostRowsTests();
    RunDeviceRowsTests();

    std::printf("--- ui: what a file transfer looks like while it runs ---\n");
    RunTransferViewTests();

    std::printf("--- ui: recent devices list (parse, touch, cap) ---\n");
    RunRecentDevicesTests();

    std::printf("--- ui: persisted share settings ---\n");
    RunUiSettingsTests();

    std::printf("--- ui: launch-at-login artifacts ---\n");
    RunAutostartConfigTests();

    std::printf("--- ui: waiting for the desktop before an automatic share ---\n");
    RunAutoShareGateTests();

    std::printf("--- ui: passcodes stored on disk ---\n");
    RunSecretTextTests();

    std::printf("--- cli: the command line every desktop shares ---\n");
    RunCliCommandTests();
    RunCliJsonTests();

    std::printf("--- fuzz: deterministic structured fuzzing over every parser ---\n");
    RunFuzzTests();

    if (g_failures == 0) {
        std::printf("=== PASS: all checks passed ===\n");
        return 0;
    }
    std::printf("=== FAIL: %d checks failed ===\n", g_failures);
    return 1;
}
