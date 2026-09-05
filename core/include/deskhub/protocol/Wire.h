#pragma once
#include "deskhub/net/TrustStore.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr uint8_t kProtocolVersion = 2;
inline constexpr size_t kMaxDatagram = 1200;
inline constexpr size_t kCommonHeaderSize = 8;
inline constexpr size_t kVideoHeaderSize = 16;
inline constexpr size_t kFecHeaderSize = 16;

inline constexpr uint16_t kFecGroupSize = 8;

inline constexpr size_t kMaxFecGroups = 256;

inline constexpr size_t kMaxSignalledFecGroups = 255;

inline constexpr size_t FecGroupCount(uint16_t pktCount, uint8_t signalled) {
    if (pktCount == 0) return 0;
    const size_t derived = (size_t(pktCount) + kFecGroupSize - 1) / kFecGroupSize;
    const size_t want = signalled ? size_t(signalled) : derived;
    return want < size_t(pktCount) ? want : size_t(pktCount);
}

inline constexpr size_t kFecLenPrefix = 2;
inline constexpr size_t kMaxVideoPayload =
    kMaxDatagram - kCommonHeaderSize - kFecHeaderSize - kFecLenPrefix;

inline constexpr size_t kAudioHeaderSize = 12;
inline constexpr size_t kMaxAudioPayload =
    kMaxDatagram - kCommonHeaderSize - kAudioHeaderSize;

inline constexpr size_t kInputHeaderSize = 5;
inline constexpr size_t kInputEventSize = 19;
inline constexpr size_t kMaxInputEvents =
    (kMaxDatagram - kCommonHeaderSize - kInputHeaderSize) / kInputEventSize;

inline constexpr size_t kNackHeaderSize = 5;
inline constexpr size_t kMaxNackIndices = 255;
static_assert(kCommonHeaderSize + kNackHeaderSize + 2 * kMaxNackIndices <= kMaxDatagram);

inline constexpr size_t kClipboardHeaderSize = 8;
inline constexpr size_t kMaxClipboardChunkPayload =
    kMaxDatagram - kCommonHeaderSize - kClipboardHeaderSize;
inline constexpr size_t kMaxClipboardTextBytes = 32768;
inline constexpr size_t kMaxClipboardChunks =
    (kMaxClipboardTextBytes + kMaxClipboardChunkPayload - 1) / kMaxClipboardChunkPayload;

enum class Chan : uint8_t { Control = 0,
    Video = 1,
    Input = 2,
    Audio = 3,
    Terminal = 4,
    File = 5 };

inline constexpr size_t kChanCount = 6;

enum class MsgType : uint8_t {
    Hello = 0x01,
    HelloAck = 0x02,
    Start = 0x03,
    Bye = 0x04,
    ListSources = 0x05,
    SourceList = 0x06,
    VideoPacket = 0x10,
    FecPacket = 0x11,
    AudioPacket = 0x12,
    InputEvent = 0x20,
    Ping = 0x30,
    Pong = 0x31,
    Feedback = 0x32,
    RequestKeyframe = 0x33,
    Reconfig = 0x34,
    SetFocus = 0x35,
    Nack = 0x36,
    InvalidateRef = 0x37,
    Clipboard = 0x40,
    TermOpen = 0x50,
    TermOpenAck = 0x51,
    TermData = 0x52,
    TermResize = 0x53,
    TermClose = 0x54,
    TermExit = 0x55,
    AuthStart = 0x60,
    AuthChallenge = 0x61,
    AuthResponse = 0x62,
    AuthResult = 0x63,
    FileOffer = 0x70,
    FileAccept = 0x71,
    FileChunk = 0x72,
    FileDone = 0x73,
    FileAck = 0x74,
    FileCancel = 0x75,
};

inline constexpr uint8_t kVideoFlagIdr = 1u << 0;
inline constexpr uint8_t kVideoFlagFrameEnd = 1u << 1;

inline constexpr uint8_t kFecParityIndexShift = 2;
inline constexpr size_t kMaxParityPerGroup = 64;

inline constexpr uint16_t kCodecMaskH264 = 1u << 0;
inline constexpr uint16_t kCodecMaskH264High444 = 1u << 1;
inline constexpr uint16_t kCodecMaskHevc = 1u << 2;
inline constexpr uint16_t kCodecMaskAv1 = 1u << 3;

enum class Codec : uint8_t { H264 = 0,
    H264High444 = 1,
    Hevc = 2,
    Av1 = 3,
    Rejected = 0xFF };

struct CommonHeader {
    uint8_t ver;
    MsgType type;
    uint8_t flags;
    Chan chan;
    uint32_t sessionId;
};

inline constexpr uint16_t kDeskhubPort = 47777;
inline constexpr size_t kMaxSources = 8;
inline constexpr size_t kMaxSourceNameBytes = 64;

struct SourceInfo {
    uint8_t sourceId = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    std::string name{};
};

inline constexpr uint8_t kHostAcceptsInput = 1u << 0;
inline constexpr uint8_t kHostSharesTerminal = 1u << 1;
inline constexpr uint8_t kHostSharesAudio = 1u << 2;
inline constexpr uint8_t kHostAcceptsFiles = 1u << 3;

struct HostCaps {
    bool acceptsInput = false;
    bool terminal = false;
    bool audio = false;
    bool files = false;
};

inline constexpr uint8_t HostCapFlags(const HostCaps& caps) {
    return uint8_t((caps.acceptsInput ? kHostAcceptsInput : 0) |
                   (caps.terminal ? kHostSharesTerminal : 0) |
                   (caps.audio ? kHostSharesAudio : 0) |
                   (caps.files ? kHostAcceptsFiles : 0));
}

inline constexpr HostCaps HostCapsOfFlags(uint8_t flags) {
    return HostCaps{(flags & kHostAcceptsInput) != 0, (flags & kHostSharesTerminal) != 0,
        (flags & kHostSharesAudio) != 0, (flags & kHostAcceptsFiles) != 0};
}

inline constexpr uint16_t kClientWantsAudio = 1u << 0;

inline constexpr size_t kMaxClientNameBytes = 64;

inline constexpr size_t kPasscodeDigits = 4;

inline constexpr bool IsValidPasscode(std::string_view p) {
    if (p.size() != kPasscodeDigits) return false;
    for (char c : p)
        if (c < '0' || c > '9') return false;
    return true;
}

inline constexpr uint32_t kPasscodeRange = 10000;

inline std::string PasscodeFromRandom(uint32_t value) {
    std::string out(kPasscodeDigits, '0');
    uint32_t left = value % kPasscodeRange;
    for (size_t i = kPasscodeDigits; i-- > 0;) {
        out[i] = char('0' + left % 10);
        left /= 10;
    }
    return out;
}

struct Hello {
    uint32_t clientId;
    uint16_t codecMask;
    uint16_t maxWidth;
    uint16_t maxHeight;
    uint8_t desiredFps;
    uint16_t features;
    uint8_t sourceId = 0;
    std::string passcode{};
    std::string clientName{};
};

enum class RejectReason : uint8_t {
    None = 0,
    Busy = 1,
    CodecMismatch = 2,
    WrongPasscode = 3,
};

enum class KeyframeReason : uint8_t {
    Loss = 0,
    WaitIdr = 1,
    QOverflow = 2,
    DecFail = 3,
    DisplayCongested = 4,
    ViewerJoin = 5,
    Unknown = 6,
};

inline constexpr size_t kKeyframeReasonCount = 7;

struct HelloAck {
    uint32_t sessionId;
    Codec codec;
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint32_t bitrateBps;
    uint64_t timebaseUs;
    RejectReason reason = RejectReason::None;
};

inline constexpr size_t kAuthNonceBytes = 32;
inline constexpr size_t kAuthSaltBytes = 16;
inline constexpr size_t kAuthMacBytes = 32;
inline constexpr size_t kMaxAuthBlobBytes = 256;

enum class AuthMode : uint8_t {
    Denied = 0,
    Signature = 1,
    Passcode = 2,
    Approval = 3,
};

enum class AuthResultCode : uint8_t {
    Accepted = 0,
    WrongPasscode = 1,
    NotPaired = 2,
    PairingDisabled = 3,
    Refused = 4,
    TimedOut = 5,
    Locked = 6,
};

struct AuthStart {
    std::vector<uint8_t> publicKey{};
    std::string clientName{};
    bool hasPasscode = false;
};

struct AuthChallenge {
    AuthMode mode = AuthMode::Denied;
    std::array<uint8_t, kAuthNonceBytes> nonce{};
    std::array<uint8_t, kAuthSaltBytes> salt{};
    std::vector<uint8_t> spake{};
};

struct AuthResponse {
    std::vector<uint8_t> proof{};
    std::array<uint8_t, kAuthMacBytes> confirm{};
};

struct AuthResult {
    AuthResultCode code = AuthResultCode::NotPaired;
    std::array<uint8_t, kAuthMacBytes> confirm{};
};

struct PingPong {
    uint32_t pingId;
    uint64_t sendTimeUs;
    uint64_t hostTimeUs = 0;
};

struct Feedback {
    uint16_t lostFrames;
    uint8_t lossPct;
    uint16_t rttMs;
    uint32_t recvBitrateKbps;
};

struct Reconfig {
    uint16_t width;
    uint16_t height;
    uint32_t bitrateBps;
    uint8_t fps = 0;
};

enum class InputType : uint8_t {
    Key = 1,
    MouseMove = 2,
    MouseButton = 3,
    MouseWheel = 4,
};

enum class MouseButton : uint8_t { Left = 1,
    Right = 2,
    Middle = 3,
    X1 = 4,
    X2 = 5 };

inline constexpr int32_t kScanExtended = 0x100;

struct InputEvent {
    InputType type = InputType::Key;
    uint64_t timestampUs = 0;
    int32_t a = 0;
    int32_t b = 0;
    uint8_t state = 0;
    uint8_t absolute = 0;
};

inline constexpr bool IsStateEvent(InputType t) {
    return t == InputType::Key || t == InputType::MouseButton;
}

struct VideoHeader {
    uint32_t frameId;
    uint64_t timestampUs;
    uint16_t pktIndex;
    uint16_t pktCount;
};

struct VideoPacketView {
    VideoHeader hdr;
    bool idr;
    bool frameEnd;
    std::span<const uint8_t> payload;
};

struct FecHeader {
    uint32_t frameId;
    uint64_t timestampUs;
    uint16_t pktCount;
    uint8_t groupIndex;
    uint8_t groups = 0;
    uint8_t parityIndex = 0;
};

struct FecPacketView {
    FecHeader hdr;
    bool idr;
    std::span<const uint8_t> parity;
};

struct AudioHeader {
    uint32_t seq;
    uint64_t timestampUs;
};

struct AudioPacketView {
    AudioHeader hdr;
    std::span<const uint8_t> payload;
};

struct ClipboardChunkView {
    uint32_t revision = 0;
    uint16_t chunkIndex = 0;
    uint16_t chunkCount = 0;
    std::span<const uint8_t> payload{};
};

size_t BuildHello(std::span<uint8_t> out, const Hello& m);
size_t BuildHelloAck(std::span<uint8_t> out, const HelloAck& m);
size_t BuildAuthStart(std::span<uint8_t> out, const AuthStart& m);
size_t BuildAuthChallenge(std::span<uint8_t> out, const AuthChallenge& m);
size_t BuildAuthResponse(std::span<uint8_t> out, const AuthResponse& m);
size_t BuildAuthResult(std::span<uint8_t> out, const AuthResult& m);
size_t BuildStart(std::span<uint8_t> out, uint32_t sessionId);
size_t BuildListSources(std::span<uint8_t> out, std::string_view passcode = {});
size_t BuildSourceList(std::span<uint8_t> out, std::span<const SourceInfo> sources,
    HostCaps caps = {});
size_t BuildBye(std::span<uint8_t> out, uint32_t sessionId);
size_t BuildPing(std::span<uint8_t> out, uint32_t sessionId, const PingPong& m);
size_t BuildPong(std::span<uint8_t> out, uint32_t sessionId, const PingPong& m);
size_t BuildFeedback(std::span<uint8_t> out, uint32_t sessionId, const Feedback& m);
size_t BuildRequestKeyframe(std::span<uint8_t> out, uint32_t sessionId, KeyframeReason reason);
size_t BuildReconfig(std::span<uint8_t> out, uint32_t sessionId, const Reconfig& m);
size_t BuildSetFocus(std::span<uint8_t> out, uint32_t sessionId, bool focused);
size_t BuildNack(std::span<uint8_t> out, uint32_t sessionId, uint32_t frameId,
    std::span<const uint16_t> indices);
size_t BuildInvalidateRef(std::span<uint8_t> out, uint32_t sessionId, uint32_t frameId);
size_t BuildVideoPacket(std::span<uint8_t> out, uint32_t sessionId, const VideoHeader& vh,
    bool idr, bool frameEnd, std::span<const uint8_t> payload);
size_t BuildFecPacket(std::span<uint8_t> out, uint32_t sessionId, const FecHeader& fh,
    bool idr, std::span<const uint8_t> parity);
size_t BuildAudioPacket(std::span<uint8_t> out, uint32_t sessionId, const AudioHeader& ah,
    std::span<const uint8_t> payload);
size_t BuildInputEvents(std::span<uint8_t> out, uint32_t sessionId, uint32_t firstSeq,
    std::span<const InputEvent> events);
size_t BuildClipboardChunk(std::span<uint8_t> out, uint32_t sessionId,
    const ClipboardChunkView& chunk);

std::optional<CommonHeader> ParseCommonHeader(std::span<const uint8_t> datagram);
std::span<const uint8_t> PayloadOf(std::span<const uint8_t> datagram);

std::optional<Hello> ParseHello(std::span<const uint8_t> payload);
std::string ParseListSourcesPasscode(std::span<const uint8_t> payload);
size_t ParseSourceList(std::span<const uint8_t> payload, std::span<SourceInfo> out);
std::optional<HelloAck> ParseHelloAck(std::span<const uint8_t> payload);
std::optional<AuthStart> ParseAuthStart(std::span<const uint8_t> payload);
std::optional<AuthChallenge> ParseAuthChallenge(std::span<const uint8_t> payload);
std::optional<AuthResponse> ParseAuthResponse(std::span<const uint8_t> payload);
std::optional<AuthResult> ParseAuthResult(std::span<const uint8_t> payload);
std::optional<PingPong> ParsePingPong(std::span<const uint8_t> payload);
std::optional<Feedback> ParseFeedback(std::span<const uint8_t> payload);
std::optional<Reconfig> ParseReconfig(std::span<const uint8_t> payload);
KeyframeReason ParseRequestKeyframe(std::span<const uint8_t> payload);

std::optional<bool> ParseSetFocus(std::span<const uint8_t> payload);
size_t ParseNack(std::span<const uint8_t> payload, uint32_t& frameId,
    std::span<uint16_t> out);
std::optional<uint32_t> ParseInvalidateRef(std::span<const uint8_t> payload);
std::optional<VideoPacketView> ParseVideoPacket(const CommonHeader& h,
    std::span<const uint8_t> payload);
std::optional<FecPacketView> ParseFecPacket(const CommonHeader& h,
    std::span<const uint8_t> payload);
std::optional<AudioPacketView> ParseAudioPacket(std::span<const uint8_t> payload);
size_t ParseInputEvents(std::span<const uint8_t> payload, uint32_t& firstSeq,
    std::span<InputEvent> out);
std::optional<ClipboardChunkView> ParseClipboardChunk(std::span<const uint8_t> payload);
std::string TruncateClipboardText(std::string_view text);

inline constexpr size_t kRecordPrefixSize = 2;
inline constexpr size_t kMaxRecordSize = 16384;

enum class RecordStatus : uint8_t { Ok = 0,
    NeedMore = 1,
    Invalid = 2 };

struct RecordView {
    RecordStatus status = RecordStatus::NeedMore;
    std::span<const uint8_t> message{};
    size_t consumed = 0;
};

size_t BuildRecord(std::span<uint8_t> out, std::span<const uint8_t> message);
RecordView ReadRecord(std::span<const uint8_t> buffer);

enum class PacketKind : uint8_t { Unknown = 0,
    Quic = 1,
    Deskhub = 2 };

PacketKind ClassifyPacket(std::span<const uint8_t> datagram);

inline constexpr size_t kMaxTermDataBytes = 4096;
inline constexpr uint16_t kMinTermCols = 1;
inline constexpr uint16_t kMinTermRows = 1;
inline constexpr uint16_t kMaxTermCols = 1000;
inline constexpr uint16_t kMaxTermRows = 1000;
inline constexpr uint16_t kDefaultTermCols = 80;
inline constexpr uint16_t kDefaultTermRows = 24;

struct TermSize {
    uint16_t cols = kDefaultTermCols;
    uint16_t rows = kDefaultTermRows;

    bool operator==(const TermSize&) const = default;
};

bool IsValidTermSize(TermSize size);
TermSize ClampTermSize(TermSize size);

struct TermOpen {
    TermSize size{};
    uint32_t resumeId = 0;
    std::string passcode{};
    std::string clientName{};
};

enum class TermReason : uint8_t {
    Accepted = 0,
    WrongPasscode = 1,
    TooManySessions = 2,
    NotShared = 3,
    NoSuchSession = 4,
};

struct TermOpenAck {
    uint32_t termId = 0;
    TermReason reason = TermReason::Accepted;
    bool resumed = false;
};

size_t BuildTermOpen(std::span<uint8_t> out, const TermOpen& m);
size_t BuildTermOpenAck(std::span<uint8_t> out, const TermOpenAck& m);
size_t BuildTermData(std::span<uint8_t> out, uint32_t termId, std::span<const uint8_t> data);
size_t BuildTermResize(std::span<uint8_t> out, uint32_t termId, TermSize size);
size_t BuildTermClose(std::span<uint8_t> out, uint32_t termId);
size_t BuildTermExit(std::span<uint8_t> out, uint32_t termId, int32_t exitCode);

std::optional<TermOpen> ParseTermOpen(std::span<const uint8_t> payload);
std::optional<TermOpenAck> ParseTermOpenAck(std::span<const uint8_t> payload);
std::optional<TermSize> ParseTermResize(std::span<const uint8_t> payload);
std::optional<int32_t> ParseTermExit(std::span<const uint8_t> payload);

inline constexpr size_t kMaxTransferFiles = 32;
inline constexpr size_t kMaxTransferNameBytes = 255;
inline constexpr uint64_t kMaxTransferFileBytes = 8ull << 30;
inline constexpr uint64_t kMaxTransferBatchBytes = 32ull << 30;

inline constexpr size_t kFileOfferHeaderSize = 5;
inline constexpr size_t kFileOfferEntryOverhead = 9;
static_assert(kCommonHeaderSize + kFileOfferHeaderSize +
                  kMaxTransferFiles * (kFileOfferEntryOverhead + kMaxTransferNameBytes) <=
              kMaxRecordSize);

inline constexpr size_t kFileChunkHeaderSize = 14;
inline constexpr size_t kMaxFileChunkBytes =
    kMaxRecordSize - kCommonHeaderSize - kFileChunkHeaderSize;

bool IsWireLegalFileName(std::string_view name);

struct TransferFile {
    uint64_t size = 0;
    std::string name{};
};

struct FileOffer {
    uint32_t batchId = 0;
    std::vector<TransferFile> files{};
};

enum class TransferReason : uint8_t {
    Accepted = 0,
    NotAccepting = 1,
    Busy = 2,
    TooManyFiles = 3,
    TooLarge = 4,
    BadName = 5,
    WriteFailed = 6,
    Corrupt = 7,
    Cancelled = 8,
    LinkLost = 9,
    ReadFailed = 10,
};

inline constexpr uint8_t kMaxTransferReason = uint8_t(TransferReason::ReadFailed);

struct FileAccept {
    uint32_t batchId = 0;
    TransferReason reason = TransferReason::Accepted;
};

struct FileChunkView {
    uint32_t batchId = 0;
    uint16_t fileIndex = 0;
    uint64_t offset = 0;
    std::span<const uint8_t> data{};
};

struct FileDone {
    uint32_t batchId = 0;
    uint16_t fileIndex = 0;
    uint32_t crc32 = 0;
};

struct FileAck {
    uint32_t batchId = 0;
    uint16_t fileIndex = 0;
    TransferReason reason = TransferReason::Accepted;
};

struct FileCancel {
    uint32_t batchId = 0;
    TransferReason reason = TransferReason::Cancelled;
};

size_t BuildFileOffer(std::span<uint8_t> out, const FileOffer& m);
size_t BuildFileAccept(std::span<uint8_t> out, const FileAccept& m);
size_t BuildFileChunk(std::span<uint8_t> out, uint32_t batchId, uint16_t fileIndex,
    uint64_t offset, std::span<const uint8_t> data);
size_t BuildFileDone(std::span<uint8_t> out, const FileDone& m);
size_t BuildFileAck(std::span<uint8_t> out, const FileAck& m);
size_t BuildFileCancel(std::span<uint8_t> out, const FileCancel& m);

std::optional<FileOffer> ParseFileOffer(std::span<const uint8_t> payload);
std::optional<FileAccept> ParseFileAccept(std::span<const uint8_t> payload);
std::optional<FileChunkView> ParseFileChunk(std::span<const uint8_t> payload);
std::optional<FileDone> ParseFileDone(std::span<const uint8_t> payload);
std::optional<FileAck> ParseFileAck(std::span<const uint8_t> payload);
std::optional<FileCancel> ParseFileCancel(std::span<const uint8_t> payload);

}
