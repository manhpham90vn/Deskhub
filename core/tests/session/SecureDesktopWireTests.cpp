#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/ByteOrder.h"
#include "deskhub/session/host/SecureDesktopWire.h"

#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

std::span<const uint8_t> PayloadOf(const std::vector<uint8_t>& message) {
    return std::span<const uint8_t>(message).subspan(kSecureDesktopHeaderSize);
}

bool NextFrom(const std::vector<uint8_t>& message, SecureDesktopMessage& out) {
    SecureDesktopStream stream;
    stream.Append(message);
    return stream.Next(out);
}

void TestDesktopNames() {
    std::printf("[securedesk] only the user's own desktop counts as normal...\n");
    Check(!IsSecureDesktopName("Default"), "Default is the normal desktop");
    Check(!IsSecureDesktopName("default"), "desktop names ignore case");
    Check(!IsSecureDesktopName("DEFAULT"), "desktop names ignore case in capitals too");
    Check(IsSecureDesktopName("Winlogon"), "Winlogon (UAC, lock screen) is secure");
    Check(IsSecureDesktopName("Screen-saver"), "the screen-saver desktop is not the user's");
    Check(IsSecureDesktopName("Defaulx"), "a same-length name that differs is not Default");
    Check(!IsSecureDesktopName(""), "an unknown desktop is not reported as secure");
}

void TestWatchRoundTrip() {
    std::printf("[securedesk] a watched monitor keeps negative origins...\n");
    SecureDesktopWatch w;
    w.sourceId = 3;
    w.left = -1920;
    w.top = -200;
    w.width = 1920;
    w.height = 1080;
    SecureDesktopMessage msg;
    Check(NextFrom(EncodeSecureDesktopWatch(w), msg), "a watch message is framed");
    Check(msg.type == SecureDesktopMsg::Watch, "it is read back as a watch");
    const auto back = DecodeSecureDesktopWatch(msg.payload);
    Check(back.has_value(), "a watch decodes");
    Check(back && back->sourceId == 3 && back->left == -1920 && back->top == -200 &&
              back->width == 1920 && back->height == 1080,
        "every watch field round-trips");
    Check(back && !back->alwaysStream && back->maxFps == kSecureDesktopDefaultMaxFps,
        "a plain watch streams only the secure desktop at the default cap");

    w.alwaysStream = true;
    w.maxFps = 60;
    const auto always = DecodeSecureDesktopWatch(PayloadOf(EncodeSecureDesktopWatch(w)));
    Check(always && always->alwaysStream && always->maxFps == 60,
        "an unattended watch asks for every desktop at the share fps");
    w.maxFps = 0;
    Check(!DecodeSecureDesktopWatch(PayloadOf(EncodeSecureDesktopWatch(w))),
        "a zero fps cap is refused");
    w.maxFps = kSecureDesktopMaxFps + 1;
    Check(!DecodeSecureDesktopWatch(PayloadOf(EncodeSecureDesktopWatch(w))),
        "an fps cap past the settings limit is refused");
    w.maxFps = 60;
    std::vector<uint8_t> badFlag = EncodeSecureDesktopWatch(w);
    badFlag[kSecureDesktopHeaderSize + 17] = 2;
    Check(!DecodeSecureDesktopWatch(PayloadOf(badFlag)),
        "a stream flag other than 0 or 1 is refused");

    w.width = 0;
    Check(!DecodeSecureDesktopWatch(PayloadOf(EncodeSecureDesktopWatch(w))),
        "a zero-sized monitor is refused");
    w.width = kSecureDesktopMaxFrameSide + 1;
    Check(!DecodeSecureDesktopWatch(PayloadOf(EncodeSecureDesktopWatch(w))),
        "an oversized monitor is refused");
    Check(!DecodeSecureDesktopWatch(std::vector<uint8_t>(5)), "a short watch is refused");
}

void TestRequestBytes() {
    std::printf("[securedesk] the first byte on the control pipe names the request...\n");
    Check(DecodeSecureDesktopRequest(1) == SecureDesktopRequest::Agent, "1 asks for an agent");
    Check(DecodeSecureDesktopRequest(2) == SecureDesktopRequest::Takeover,
        "2 takes the machine over from the unattended host");
    Check(DecodeSecureDesktopRequest(3) == SecureDesktopRequest::Configure,
        "3 carries the unattended configuration");
    Check(DecodeSecureDesktopRequest(4) == SecureDesktopRequest::Status,
        "4 asks how the unattended host is doing");
    Check(!DecodeSecureDesktopRequest(0) && !DecodeSecureDesktopRequest(5),
        "anything else is refused before the service does any work");
}

UnattendedConfig SampleConfig() {
    UnattendedConfig config;
    config.enabled = true;
    config.hostKeyPem = "-----BEGIN PRIVATE KEY-----\nabc\n-----END PRIVATE KEY-----\n";
    config.authorizedKeys = "ssh-ed25519 AAAA laptop\n";
    config.settings = "port=4200\nfps=30\n";
    return config;
}

std::span<const uint8_t> BodyOf(const std::vector<uint8_t>& encoded) {
    return std::span<const uint8_t>(encoded).subspan(kUnattendedConfigLengthSize);
}

void TestUnattendedConfigRoundTrip() {
    std::printf("[securedesk] the unattended configuration is length-prefixed blocks...\n");
    const UnattendedConfig config = SampleConfig();
    const std::vector<uint8_t> encoded = EncodeUnattendedConfig(config);
    Check(!encoded.empty(), "a complete configuration encodes");
    const auto length = DecodeUnattendedConfigLength(
        std::span<const uint8_t>(encoded).first(kUnattendedConfigLengthSize));
    Check(length && *length == encoded.size() - kUnattendedConfigLengthSize,
        "the prefix is the body length");
    Check(DecodeUnattendedConfig(BodyOf(encoded)) == config, "every block round-trips");

    UnattendedConfig off;
    const std::vector<uint8_t> offBytes = EncodeUnattendedConfig(off);
    Check(DecodeUnattendedConfig(BodyOf(offBytes)) == off,
        "switching off carries no key and no list");

    UnattendedConfig keyless = config;
    keyless.hostKeyPem.clear();
    Check(EncodeUnattendedConfig(keyless).empty(), "enabled without a key is not encoded");
    std::vector<uint8_t> forged = encoded;
    forged[kUnattendedConfigLengthSize] = 1;
    PutU32(forged.data() + kUnattendedConfigLengthSize + 1, 0);
    Check(!DecodeUnattendedConfig(BodyOf(forged)), "nor decoded");
}

void TestUnattendedConfigLimits() {
    std::printf("[securedesk] oversized or truncated configurations are refused...\n");
    UnattendedConfig big = SampleConfig();
    big.authorizedKeys.assign(kUnattendedMaxAuthorizedKeysBytes + 1, 'k');
    Check(EncodeUnattendedConfig(big).empty(), "a list past the file limit is not encoded");
    big = SampleConfig();
    big.settings.assign(kUnattendedMaxSettingsBytes, 's');
    Check(!EncodeUnattendedConfig(big).empty(), "a block at its limit is fine");

    const std::vector<uint8_t> encoded = EncodeUnattendedConfig(SampleConfig());
    std::vector<uint8_t> truncated(BodyOf(encoded).begin(), BodyOf(encoded).end());
    truncated.pop_back();
    Check(!DecodeUnattendedConfig(truncated), "a body missing a byte is refused");
    std::vector<uint8_t> padded(BodyOf(encoded).begin(), BodyOf(encoded).end());
    padded.push_back(0);
    Check(!DecodeUnattendedConfig(padded), "a body with a trailing byte is refused");
    std::vector<uint8_t> badFlag(BodyOf(encoded).begin(), BodyOf(encoded).end());
    badFlag[0] = 7;
    Check(!DecodeUnattendedConfig(badFlag), "an enabled flag other than 0 or 1 is refused");
    Check(!DecodeUnattendedConfig({}), "an empty body is refused");

    uint8_t zero[kUnattendedConfigLengthSize] = {};
    Check(!DecodeUnattendedConfigLength(zero), "a zero length is refused");
    uint8_t huge[kUnattendedConfigLengthSize];
    PutU32(huge, uint32_t(kUnattendedConfigMaxBytes + 1));
    Check(!DecodeUnattendedConfigLength(huge), "a length past every block limit is refused");
    Check(!DecodeUnattendedConfigLength(std::vector<uint8_t>(3)), "a short prefix is refused");
    std::vector<uint8_t> lying(BodyOf(encoded).begin(), BodyOf(encoded).end());
    PutU32(lying.data() + 1, uint32_t(kUnattendedMaxHostKeyBytes + 1));
    Check(!DecodeUnattendedConfig(lying), "a block claiming more than its limit is refused");
}

void TestDesktopState() {
    std::printf("[securedesk] the desktop state is one byte...\n");
    SecureDesktopMessage msg;
    Check(NextFrom(EncodeSecureDesktopState(true), msg), "a state is framed");
    Check(msg.type == SecureDesktopMsg::DesktopState, "it is read back as a state");
    Check(DecodeSecureDesktopState(msg.payload) == std::optional<bool>(true), "secure round-trips");
    Check(DecodeSecureDesktopState(PayloadOf(EncodeSecureDesktopState(false))) ==
              std::optional<bool>(false),
        "normal round-trips");
    const uint8_t bogus[] = {2};
    Check(!DecodeSecureDesktopState(bogus), "a state byte other than 0 or 1 is refused");
}

void TestInputRoundTrip() {
    std::printf("[securedesk] injected input keeps signed moves and wheel data...\n");
    SecureDesktopInput in;
    in.kind = SecureDesktopInputKind::Mouse;
    in.vk = 0x41;
    in.scan = 0x1E;
    in.flags = 0x8001;
    in.dx = -12;
    in.dy = 65535;
    in.data = uint32_t(-120);
    SecureDesktopMessage msg;
    Check(NextFrom(EncodeSecureDesktopInput(in), msg), "an input is framed");
    Check(msg.type == SecureDesktopMsg::Input, "it is read back as input");
    const auto back = DecodeSecureDesktopInput(msg.payload);
    Check(back && back->kind == SecureDesktopInputKind::Mouse && back->vk == 0x41 &&
              back->scan == 0x1E && back->flags == 0x8001 && back->dx == -12 &&
              back->dy == 65535 && back->data == uint32_t(-120),
        "every input field round-trips");

    std::vector<uint8_t> bad = EncodeSecureDesktopInput(in);
    bad[kSecureDesktopHeaderSize] = 9;
    Check(!DecodeSecureDesktopInput(PayloadOf(bad)), "an unknown input kind is refused");
    Check(!DecodeSecureDesktopInput(std::vector<uint8_t>(3)), "a short input is refused");
}

void TestFrameRoundTrip() {
    std::printf("[securedesk] a frame carries exactly width x height BGRA pixels...\n");
    std::vector<uint8_t> pixels(2 * 3 * 4);
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = uint8_t(i);
    SecureDesktopMessage msg;
    Check(NextFrom(EncodeSecureDesktopFrame(4, 2, 3, pixels), msg), "a frame is framed");
    Check(msg.type == SecureDesktopMsg::Frame, "it is read back as a frame");
    const auto frame = DecodeSecureDesktopFrame(msg.payload);
    Check(frame && frame->sourceId == 4 && frame->width == 2 && frame->height == 3,
        "the frame header round-trips");
    Check(frame && std::vector<uint8_t>(frame->bgra.begin(), frame->bgra.end()) == pixels,
        "the pixels round-trip");

    Check(EncodeSecureDesktopFrame(0, 2, 3, std::span<const uint8_t>(pixels).first(5)).empty(),
        "a frame whose pixels do not match its size is not encoded");
    Check(EncodeSecureDesktopFrame(0, 0, 3, {}).empty(), "an empty frame is not encoded");

    std::vector<uint8_t> shortFrame = EncodeSecureDesktopFrame(0, 2, 3, pixels);
    shortFrame.pop_back();
    Check(!DecodeSecureDesktopFrame(PayloadOf(shortFrame)), "a frame missing a byte is refused");
    Check(!DecodeSecureDesktopFrame(std::vector<uint8_t>(4)), "a frame without a header is refused");

    std::vector<uint8_t> huge(kSecureDesktopFrameHeaderSize);
    PutU32(huge.data() + 1, kSecureDesktopMaxFrameSide + 1);
    PutU32(huge.data() + 5, 1);
    Check(!DecodeSecureDesktopFrame(huge), "a frame wider than the limit is refused");
}

void TestStreamReassemblesSplitMessages() {
    std::printf("[securedesk] the pipe may split and merge messages...\n");
    std::vector<uint8_t> bytes = EncodeSecureDesktopState(true);
    const std::vector<uint8_t> second = EncodeSecureDesktopInput(SecureDesktopInput{});
    bytes.insert(bytes.end(), second.begin(), second.end());

    SecureDesktopStream stream;
    SecureDesktopMessage msg;
    for (size_t i = 0; i < bytes.size(); ++i) {
        stream.Append(std::span<const uint8_t>(bytes).subspan(i, 1));
        if (i + 1 < EncodeSecureDesktopState(true).size())
            Check(!stream.Next(msg), "nothing is read before a message is whole");
    }
    Check(stream.Next(msg) && msg.type == SecureDesktopMsg::DesktopState,
        "the first message comes out whole");
    Check(stream.Next(msg) && msg.type == SecureDesktopMsg::Input,
        "the second message follows it");
    Check(!stream.Next(msg), "nothing is left after both");
    stream.Append({});
    Check(!stream.Next(msg) && !stream.Failed(), "an empty append changes nothing");
}

void TestStreamRejectsGarbage() {
    std::printf("[securedesk] a garbled stream fails for good...\n");
    SecureDesktopStream stream;
    const uint8_t unknown[] = {0x7F, 0, 0, 0, 0};
    stream.Append(unknown);
    SecureDesktopMessage msg;
    Check(!stream.Next(msg) && stream.Failed(), "an unknown type fails the stream");
    stream.Append(EncodeSecureDesktopState(true));
    Check(!stream.Next(msg), "a failed stream reads nothing more");

    SecureDesktopStream big;
    uint8_t header[kSecureDesktopHeaderSize] = {uint8_t(SecureDesktopMsg::Frame)};
    PutU32(header + 1, 0xFFFFFFFFu);
    big.Append(header);
    Check(!big.Next(msg) && big.Failed(), "a length beyond the largest frame fails the stream");
}

}

void RunSecureDesktopWireTests() {
    TestDesktopNames();
    TestWatchRoundTrip();
    TestRequestBytes();
    TestUnattendedConfigRoundTrip();
    TestUnattendedConfigLimits();
    TestDesktopState();
    TestInputRoundTrip();
    TestFrameRoundTrip();
    TestStreamReassemblesSplitMessages();
    TestStreamRejectsGarbage();
}
