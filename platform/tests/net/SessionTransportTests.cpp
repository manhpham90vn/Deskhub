#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/media/ShareTypes.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr uint16_t kTestPort = 47793;
constexpr int kMaxRounds = 600;

int PumpFor(deskhubp::SessionTransport& reader, deskhubp::SessionTransport& other, uint8_t* buf,
    size_t cap, NetAddr& from) {
    for (int i = 0; i < kMaxRounds; ++i) {
        const int got = reader.RecvFrom(buf, cap, from);
        if (got > 0) return got;
        NetAddr ignored;
        uint8_t drain[deskhub::kMaxDatagram];
        other.RecvFrom(drain, sizeof(drain), ignored);
    }
    return 0;
}

void TestControlTravelsOnAStream() {
    std::printf("[transport] control messages cross as whole messages over QUIC...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid(), "the host has an identity to present");
    if (!identity.Valid()) return;

    deskhubp::SessionTransport host;
    deskhubp::SessionTransport viewer;
    host.SetRecvTimeout(1);
    viewer.SetRecvTimeout(1);

    deskhubp::QuicSettings hostSettings;
    hostSettings.certPemPath = identity.certPath;
    hostSettings.keyPemPath = identity.keyPath;
    const bool listening = host.Listen(hostSettings, kTestPort, "127.0.0.1");
    Check(listening, "the host binds the shared port");
    if (!listening) return;
    Check(host.IsOpen() && host.LocalPort() == kTestPort, "and reports the port it took");

    const NetAddr target{0x7F000001u, kTestPort};
    Check(viewer.Connect(deskhubp::QuicSettings{}, target, "deskhub-test"),
        "the viewer dials the host");

    uint8_t buf[deskhub::kMaxDatagram];
    NetAddr from;
    for (int i = 0; i < kMaxRounds && !viewer.Established(target); ++i) {
        viewer.RecvFrom(buf, sizeof(buf), from);
        host.RecvFrom(buf, sizeof(buf), from);
    }
    Check(viewer.Established(target), "the TLS handshake finishes before anything is sent");
    if (!viewer.Established(target)) return;

    const auto fingerprint = viewer.PeerFingerprint(target);
    Check(fingerprint && *fingerprint == identity.fingerprint,
        "the viewer sees the fingerprint TOFU has to compare");

    uint8_t hello[deskhub::kMaxDatagram];
    const size_t helloSize = deskhub::BuildListSources(hello);
    Check(viewer.SendTo(target, hello, helloSize), "a control message is accepted");

    const int got = PumpFor(host, viewer, buf, sizeof(buf), from);
    Check(got == int(helloSize), "the host reads back exactly one whole message");
    Check(std::equal(hello, hello + helloSize, buf),
        "byte for byte, so the framing on the stream is transparent to the loop above");
    Check(from.Pack() != 0, "and it knows which viewer sent it");

    host.Close();
    viewer.Close();
}

void TestVideoRidesEncryptedDatagrams() {
    std::printf("[transport] video rides QUIC datagrams by default, and raw video is refused...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    deskhubp::SessionTransport host;
    deskhubp::SessionTransport viewer;
    host.SetRecvTimeout(1);
    viewer.SetRecvTimeout(1);

    deskhubp::QuicSettings hostSettings;
    hostSettings.certPemPath = identity.certPath;
    hostSettings.keyPemPath = identity.keyPath;
    if (!host.Listen(hostSettings, uint16_t(kTestPort + 1), "127.0.0.1")) return;

    const NetAddr target{0x7F000001u, uint16_t(kTestPort + 1)};
    if (!viewer.Connect(deskhubp::QuicSettings{}, target, "deskhub-test")) return;

    uint8_t buf[deskhub::kMaxDatagram];
    NetAddr from;
    for (int i = 0; i < kMaxRounds && !viewer.Established(target); ++i) {
        viewer.RecvFrom(buf, sizeof(buf), from);
        host.RecvFrom(buf, sizeof(buf), from);
    }
    if (!viewer.Established(target)) return;

    Check(viewer.videoPath() == deskhubp::VideoPath::QuicDatagram,
        "video takes the encrypted datagram path out of the box");
    Check(viewer.MaxDatagramSize(target) >= deskhub::kMaxDatagram,
        "and a QUIC datagram fits a whole video packet");

    deskhub::VideoHeader header{};
    header.frameId = 7;
    header.timestampUs = 1'000'000;
    header.pktIndex = 0;
    header.pktCount = 1;
    const std::vector<uint8_t> payload(200, 0x5A);
    uint8_t video[deskhub::kMaxDatagram];
    const size_t videoSize = deskhub::BuildVideoPacket(video, 1234, header, true, true, payload);
    Check(videoSize > 0, "a video packet is built");

    Check(viewer.SendTo(target, video, videoSize), "the transport accepts it");
    const int got = PumpFor(host, viewer, buf, sizeof(buf), from);
    Check(got == int(videoSize), "it arrives whole");
    Check(std::equal(video, video + videoSize, buf), "and unchanged");

    viewer.SetVideoPath(deskhubp::VideoPath::RawUdp);
    Check(viewer.SendTo(target, video, videoSize),
        "a sender forced onto the old raw path still transmits");
    const int refused = PumpFor(host, viewer, buf, sizeof(buf), from);
    Check(refused == 0, "but the receiver refuses plaintext video now that the stream is encrypted");

    host.SetVideoPath(deskhubp::VideoPath::RawUdp);
    Check(viewer.SendTo(target, video, videoSize), "with both ends forced raw, in a test rig,");
    const int legacy = PumpFor(host, viewer, buf, sizeof(buf), from);
    Check(legacy == int(videoSize) && std::equal(video, video + videoSize, buf),
        "the legacy path still works for A/B measurements");

    Check(deskhubp::VideoPathName(host.videoPath()) == deskhub::media::kVideoPathRawUdp,
        "the transport can say which leg it is on, so a measurement log cannot lie");
    Check(deskhubp::VideoPathFromName(deskhubp::VideoPathName(deskhubp::VideoPath::QuicDatagram),
              deskhubp::VideoPath::RawUdp) == deskhubp::VideoPath::QuicDatagram,
        "and the name it gives back is the one that maps to it");

    host.Close();
    viewer.Close();
}

void TestAStrangerIsStillAnsweredInThePlain() {
    std::printf("[transport] a scanner with no connection is answered as plain UDP...\n");
    if (!deskhubp::QuicAvailable()) return;

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    deskhubp::SessionTransport host;
    host.SetRecvTimeout(1);
    deskhubp::QuicSettings hostSettings;
    hostSettings.certPemPath = identity.certPath;
    hostSettings.keyPemPath = identity.keyPath;
    if (!host.Listen(hostSettings, uint16_t(kTestPort + 2), "127.0.0.1")) return;

    UdpSocket scanner;
    Check(scanner.Open(0, "127.0.0.1"), "a plain 4.x-style scanner opens a socket");
    scanner.SetRecvTimeout(50);

    uint8_t probe[deskhub::kMaxDatagram];
    const size_t probeSize = deskhub::BuildListSources(probe);
    const NetAddr target{0x7F000001u, uint16_t(kTestPort + 2)};
    Check(scanner.SendTo(target, probe, probeSize), "it sends LIST_SOURCES with no handshake");

    uint8_t buf[deskhub::kMaxDatagram];
    NetAddr from;
    int got = 0;
    for (int i = 0; i < kMaxRounds && got == 0; ++i) got = host.RecvFrom(buf, sizeof(buf), from);
    Check(got == int(probeSize), "the beacon still reaches the host through the QUIC port");

    Check(host.SendTo(from, buf, size_t(got)),
        "and the reply goes back out as plain UDP, because the stranger has no connection");
    uint8_t back[deskhub::kMaxDatagram];
    NetAddr replyFrom;
    const int returned = scanner.RecvFrom(back, sizeof(back), replyFrom);
    Check(returned == int(probeSize), "so a scanner that speaks no QUIC still gets an answer");

    uint8_t bye[deskhub::kMaxDatagram];
    const size_t byeSize = deskhub::BuildBye(bye, 1234);
    Check(scanner.SendTo(target, bye, byeSize),
        "the scanner then tries a session message with no connection");
    NetAddr byeFrom;
    int leaked = 0;
    for (int i = 0; i < 200 && leaked == 0; ++i) leaked = host.RecvFrom(buf, sizeof(buf), byeFrom);
    Check(leaked == 0, "and the transport drops it: only beacon traffic crosses in the plain");

    scanner.Close();
    host.Close();
}

struct Link {
    deskhubp::SessionTransport host{};
    deskhubp::SessionTransport viewer{};
    NetAddr target{};

    bool Open(const deskhubp::HostIdentity& identity, uint16_t port) {
        host.SetRecvTimeout(1);
        viewer.SetRecvTimeout(1);
        deskhubp::QuicSettings hostSettings;
        hostSettings.certPemPath = identity.certPath;
        hostSettings.keyPemPath = identity.keyPath;
        if (!host.Listen(hostSettings, port, "127.0.0.1")) return false;
        target = NetAddr{0x7F000001u, port};
        if (!viewer.Connect(deskhubp::QuicSettings{}, target, "deskhub-test")) return false;

        uint8_t buf[deskhub::kMaxRecordSize];
        NetAddr from;
        for (int i = 0; i < kMaxRounds && !viewer.Established(target); ++i) {
            viewer.RecvFrom(buf, sizeof(buf), from);
            host.RecvFrom(buf, sizeof(buf), from);
        }
        return viewer.Established(target);
    }

    ~Link() {
        host.Close();
        viewer.Close();
    }
};

size_t QueueFileBacklog(Link& link, int chunks, size_t payloadBytes) {
    const std::vector<uint8_t> payload(payloadBytes, 0xA5);
    std::vector<uint8_t> record(deskhub::kMaxRecordSize);
    size_t queued = 0;
    for (int i = 0; i < chunks; ++i) {
        const size_t n =
            deskhub::BuildFileChunk(record, 1, 0, uint64_t(i) * payloadBytes, payload);
        if (n == 0) break;
        if (!link.viewer.SendRecordOn(link.target, deskhubp::kQuicFileStream,
                std::span<const uint8_t>(record.data(), n)))
            break;
        ++queued;
    }
    return queued;
}

deskhub::Chan ChanOf(const uint8_t* bytes, size_t len) {
    const auto header = deskhub::ParseCommonHeader(std::span<const uint8_t>(bytes, len));
    return header ? header->chan : deskhub::Chan::Control;
}

void TestAFileBacklogNeverDelaysTheStream() {
    std::printf("[transport] a file backlog is braked, and live traffic passes it...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    bool diskBusy = true;

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    Link link;
    if (!link.Open(identity, uint16_t(kTestPort + 3))) return;
    link.host.SetBulkReady([&diskBusy] { return !diskBusy; });

    const size_t queued = QueueFileBacklog(link, 24, 512);
    Check(queued == 24, "a viewer queues a deep run of file chunks");

    uint8_t control[deskhub::kMaxDatagram];
    const size_t controlSize = deskhub::BuildListSources(control);
    Check(link.viewer.SendTo(link.target, control, controlSize),
        "a control message follows the backlog onto the wire");

    deskhub::VideoHeader vh{};
    vh.frameId = 3;
    vh.pktCount = 1;
    uint8_t video[deskhub::kMaxDatagram];
    const size_t videoSize =
        deskhub::BuildVideoPacket(video, 77, vh, true, true, std::vector<uint8_t>(200, 0x11));
    Check(link.viewer.SendTo(link.target, video, videoSize), "and so does a video packet");

    uint8_t buf[deskhub::kMaxRecordSize];
    NetAddr from;
    bool sawControl = false;
    bool sawVideo = false;
    bool sawFile = false;
    for (int i = 0; i < 2; ++i) {
        const int got = PumpFor(link.host, link.viewer, buf, sizeof(buf), from);
        if (got <= 0) break;
        const deskhub::Chan chan = ChanOf(buf, size_t(got));
        if (chan == deskhub::Chan::Control) sawControl = true;
        if (chan == deskhub::Chan::Video) sawVideo = true;
        if (chan == deskhub::Chan::File) sawFile = true;
    }
    Check(sawControl && sawVideo,
        "both the control message and the video packet are read before anything else");
    Check(!sawFile, "not one of the 24 file chunks was allowed to get in front of them");
    Check(link.host.BulkQueued() == 0,
        "and with the disk behind, nothing was pulled off the file stream at all, so the "
        "sender is braked by QUIC flow control instead of the host growing a queue");

    diskBusy = false;
    const int backlog = PumpFor(link.host, link.viewer, buf, sizeof(buf), from);
    Check(backlog > 0 && ChanOf(buf, size_t(backlog)) == deskhub::Chan::File,
        "once the disk keeps up the file backlog is taken in");
}

void TestTheFileLaneNeverGrowsPastItsCap() {
    std::printf("[transport] the file lane is capped, however hard a sender pushes...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    Link link;
    if (!link.Open(identity, uint16_t(kTestPort + 4))) return;

    uint8_t buf[deskhub::kMaxRecordSize];
    NetAddr from;
    size_t deepest = 0;
    size_t delivered = 0;
    size_t sent = 0;

    for (int round = 0; round < 12; ++round) {
        sent += QueueFileBacklog(link, 64, 4096);
        for (int i = 0; i < 200; ++i) {
            if (link.host.BulkQueued() > deepest) deepest = link.host.BulkQueued();
            const int got = link.host.RecvFrom(buf, sizeof(buf), from);
            if (got > 0 && ChanOf(buf, size_t(got)) == deskhub::Chan::File) ++delivered;
            NetAddr ignored;
            uint8_t drain[deskhub::kMaxRecordSize];
            link.viewer.RecvFrom(drain, sizeof(drain), ignored);
        }
    }

    Check(sent > deskhubp::kMaxBulkQueued * 2,
        "the sender pushed far more chunks than the lane is allowed to hold");
    Check(delivered > 0, "chunks did keep flowing through");
    Check(deepest <= deskhubp::kMaxBulkQueued,
        "yet the file lane never grew past its cap, so a slow consumer cannot be turned "
        "into unbounded memory on the receiver");
}

void TestSendBurstsStayBounded() {
    std::printf("[transport] no single flush dumps the whole congestion window...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    Link link;
    if (!link.Open(identity, uint16_t(kTestPort + 5))) return;

    uint8_t buf[deskhub::kMaxRecordSize];
    NetAddr from;
    size_t sent = 0;
    size_t delivered = 0;

    for (int round = 0; round < 16; ++round) {
        sent += QueueFileBacklog(link, 64, deskhub::kMaxFileChunkBytes - 1);
        for (int i = 0; i < 400; ++i) {
            const int got = link.host.RecvFrom(buf, sizeof(buf), from);
            if (got > 0 && ChanOf(buf, size_t(got)) == deskhub::Chan::File) ++delivered;
            NetAddr ignored;
            uint8_t drain[deskhub::kMaxRecordSize];
            link.viewer.RecvFrom(drain, sizeof(drain), ignored);
        }
    }

    const deskhubp::QuicSendStats stats = link.viewer.SendStats();
    Check(delivered == sent, "every chunk the sender queued arrives");
    Check(stats.packets > 0, "and a real amount of traffic crossed the wire");
    Check(stats.maxBurst <= deskhubp::kMaxFlushBurst,
        "yet no single flush wrote more than the burst cap, so a bulk transfer cannot "
        "overrun a switch buffer in one go and take the live stream down with it");
    Check(stats.capped > 0,
        "and the cap really engaged, so this is measuring the brake, not an idle link");
}

void TestAudioRidesDatagramsNotTheControlStream() {
    std::printf("[transport] audio crosses as a datagram, never on the reliable stream...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    Link link;
    if (!link.Open(identity, uint16_t(kTestPort + 6))) return;

    const deskhubp::QuicSendStats before = link.viewer.SendStats();

    const std::vector<uint8_t> opus(180, 0x3C);
    const deskhub::AudioHeader ah{9, 2'000'000};
    uint8_t audio[deskhub::kMaxDatagram];
    const size_t audioSize = deskhub::BuildAudioPacket(audio, 4321, ah, opus);
    Check(audioSize > 0, "an audio packet is built");
    Check(link.viewer.SendTo(link.target, audio, audioSize), "the transport accepts it");

    const deskhubp::QuicSendStats afterAudio = link.viewer.SendStats();
    Check(afterAudio.datagrams == before.datagrams + 1,
        "it went out as one QUIC datagram, the way a lost frame is meant to stay lost");
    Check(afterAudio.streamBytes == before.streamBytes,
        "and not one byte of it was written to the reliable control stream, where a single "
        "loss would hold every later frame behind a retransmit");

    uint8_t buf[deskhub::kMaxRecordSize];
    NetAddr from;
    const int got = PumpFor(link.host, link.viewer, buf, sizeof(buf), from);
    Check(got == int(audioSize) && std::equal(audio, audio + audioSize, buf),
        "and it arrives whole");

    uint8_t control[deskhub::kMaxDatagram];
    const size_t controlSize = deskhub::BuildListSources(control);
    Check(link.viewer.SendTo(link.target, control, controlSize), "a control message follows");
    const deskhubp::QuicSendStats afterControl = link.viewer.SendStats();
    Check(afterControl.datagrams == afterAudio.datagrams,
        "control traffic did not take the datagram path");
    Check(afterControl.streamBytes > afterAudio.streamBytes,
        "it took the reliable stream, so the split is by channel and nothing else");
}

void TestAnUnopenedTransportIsHarmless() {
    std::printf("[transport] a transport that never opened refuses everything quietly...\n");
    deskhubp::SessionTransport idle;
    uint8_t buf[16];
    NetAddr from;
    Check(idle.RecvFrom(buf, sizeof(buf), from) == -1, "reading reports the closed transport");
    Check(!idle.Established(NetAddr{}), "nothing is established");
    Check(!idle.PeerFingerprint(NetAddr{}).has_value(), "and there is no peer to fingerprint");
    Check(idle.MaxDatagramSize(NetAddr{}) == 0, "no datagram budget either");
    const uint8_t byte = 1;
    Check(!idle.SendRecord(NetAddr{0x7F000001u, 9}, std::span<const uint8_t>(&byte, 1)),
        "a record to a peer that never connected is refused");
    idle.Close();
    Check(!idle.IsOpen(), "closing an unopened transport is a no-op");
}

void TestAnIdleTransportWaitsInsteadOfSpinning() {
    std::printf("[transport] an idle transport serves its timeout instead of spinning...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[transport] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    deskhubp::SessionTransport host;
    deskhubp::QuicSettings settings;
    settings.certPemPath = identity.certPath;
    settings.keyPemPath = identity.keyPath;
    const bool listening = host.Listen(settings, kTestPort, "127.0.0.1");
    Check(listening, "the host binds the shared port");
    if (!listening) return;
    host.SetRecvTimeout(50);

    uint8_t buf[deskhub::kMaxDatagram];
    NetAddr from;
    const uint64_t t0 = NowUs();
    const int got = host.RecvFrom(buf, sizeof(buf), from);
    const uint64_t elapsedUs = NowUs() - t0;
    Check(got <= 0, "with nobody connected there is nothing to read");
    Check(elapsedUs >= 25'000,
        "and the call actually waited, so the net loop does not spin at 100% CPU");

    const uint8_t byte = 1;
    Check(!host.SendRecord(NetAddr{0x7F000001u, 9}, std::span<const uint8_t>(&byte, 1)),
        "an open transport still refuses a record to a peer that never connected");
    host.Close();
}

}

void RunSessionTransportTests() {
    TestControlTravelsOnAStream();
    TestVideoRidesEncryptedDatagrams();
    TestAStrangerIsStillAnsweredInThePlain();
    TestAFileBacklogNeverDelaysTheStream();
    TestTheFileLaneNeverGrowsPastItsCap();
    TestSendBurstsStayBounded();
    TestAudioRidesDatagramsNotTheControlStream();
    TestAnUnopenedTransportIsHarmless();
    TestAnIdleTransportWaitsInsteadOfSpinning();
}
