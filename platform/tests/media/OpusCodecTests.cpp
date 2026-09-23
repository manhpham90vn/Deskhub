#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transport/AudioJitterBuffer.h"
#include "deskhubp/audio/AudioPlayer.h"
#include "deskhubp/audio/AudioSink.h"
#include "deskhubp/media/OpusCodec.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <string_view>
#include <vector>

using namespace deskhubp;
using deskhub::media::AudioFormat;

namespace {

constexpr int kToneHz = 440;
constexpr double kTwoPi = 6.283185307179586;
constexpr int16_t kToneAmplitude = 12000;
constexpr int kPlayoutWaitTicks = 300;

std::vector<int16_t> Tone(const AudioFormat& format, int frameIndex) {
    std::vector<int16_t> pcm(format.interleavedSamples());
    const size_t first = size_t(frameIndex) * format.samplesPerFrame;
    for (uint32_t i = 0; i < format.samplesPerFrame; ++i) {
        const double t = double(first + i) / format.sampleRate;
        const auto s = int16_t(kToneAmplitude * std::sin(kTwoPi * kToneHz * t));
        for (uint32_t c = 0; c < format.channels; ++c) pcm[size_t(i) * format.channels + c] = s;
    }
    return pcm;
}

double Rms(const std::vector<int16_t>& pcm) {
    double sum = 0.0;
    for (int16_t s : pcm) sum += double(s) * double(s);
    return pcm.empty() ? 0.0 : std::sqrt(sum / double(pcm.size()));
}

bool BuiltWithoutOpus() {
    return std::string_view(OpusAudioEncoder::BackendName()) == "none";
}

void TestRoundTrip() {
    std::printf("[audio] a tone survives encode and decode at the negotiated format...\n");
    const AudioFormat format{};
    OpusAudioEncoder enc;
    OpusAudioDecoder dec;
    if (BuiltWithoutOpus()) {
        Check(!enc.Open(format, deskhub::media::kAudioBitrateBps),
            "a build without the codec refuses to encode rather than pretending");
        return;
    }
    Check(enc.Open(format, deskhub::media::kAudioBitrateBps), "the encoder opens");
    Check(dec.Open(format), "the decoder opens");
    Check(enc.IsOpen() && dec.IsOpen(), "both report themselves open");

    std::vector<uint8_t> packet(kMaxOpusPacketBytes);
    std::vector<int16_t> out(format.interleavedSamples());
    size_t largest = 0;
    double decodedRms = 0.0;
    constexpr int kFrames = 25;

    for (int f = 0; f < kFrames; ++f) {
        const std::vector<int16_t> pcm = Tone(format, f);
        const size_t written = enc.Encode(pcm, packet);
        Check(written > 0, "every frame encodes to at least one byte");
        if (written > largest) largest = written;

        const size_t samples = dec.Decode(std::span<const uint8_t>(packet.data(), written), out);
        Check(samples == format.samplesPerFrame, "the decoder returns exactly one frame");
        if (f == kFrames - 1) decodedRms = Rms(out);
    }

    Check(largest <= deskhub::kMaxAudioPayload,
        "the widest packet still fits the audio payload of one datagram");
    const double sourceRms = Rms(Tone(format, kFrames - 1));
    Check(std::fabs(20.0 * std::log10(decodedRms / sourceRms)) < 2.0,
        "the decoded tone comes back at the level it went in");
}

void TestConcealmentFillsAFrame() {
    std::printf("[audio] concealment produces a whole frame without a packet...\n");
    if (BuiltWithoutOpus()) return;
    const AudioFormat format{};
    OpusAudioEncoder enc;
    OpusAudioDecoder dec;
    Check(enc.Open(format, deskhub::media::kAudioBitrateBps) && dec.Open(format),
        "the pair opens");

    std::vector<uint8_t> packet(kMaxOpusPacketBytes);
    std::vector<int16_t> out(format.interleavedSamples());
    for (int f = 0; f < 5; ++f) {
        const size_t written = enc.Encode(Tone(format, f), packet);
        dec.Decode(std::span<const uint8_t>(packet.data(), written), out);
    }

    std::vector<int16_t> concealed(format.interleavedSamples(), 0x7FFF);
    Check(dec.Conceal(concealed) == format.samplesPerFrame,
        "a lost frame is concealed at full length");
    Check(Rms(concealed) < kToneAmplitude,
        "concealment writes real samples over the buffer it was given");
}

void TestBadInputIsRefused() {
    std::printf("[audio] the codec refuses formats and buffers it cannot honour...\n");
    if (BuiltWithoutOpus()) return;
    OpusAudioEncoder enc;
    AudioFormat wrongRate{};
    wrongRate.sampleRate = 44100;
    wrongRate.samplesPerFrame = 882;
    Check(!enc.Open(wrongRate, deskhub::media::kAudioBitrateBps),
        "a rate Opus does not speak is refused at open");

    AudioFormat tooManyChannels{};
    tooManyChannels.channels = 6;
    Check(!enc.Open(tooManyChannels, deskhub::media::kAudioBitrateBps),
        "more channels than stereo is refused at open");

    const AudioFormat format{};
    std::vector<uint8_t> packet(kMaxOpusPacketBytes);
    std::vector<int16_t> shortFrame(format.interleavedSamples() / 2);
    Check(enc.Encode(shortFrame, packet) == 0, "a closed encoder encodes nothing");

    Check(enc.Open(format, deskhub::media::kAudioBitrateBps), "the encoder reopens");
    Check(enc.Encode(shortFrame, packet) == 0, "half a frame is not a frame");
    Check(enc.Encode(Tone(format, 0), std::span<uint8_t>()) == 0,
        "an empty output buffer yields nothing");
    Check(enc.SetBitrate(32000), "the bitrate can be lowered while open");

    OpusAudioDecoder dec;
    std::vector<int16_t> out(format.interleavedSamples());
    Check(dec.Decode(std::span<const uint8_t>(packet.data(), 4), out) == 0,
        "a closed decoder decodes nothing");
    Check(dec.Open(format), "the decoder opens");
    Check(dec.Decode(std::span<const uint8_t>(), out) == 0, "an empty packet decodes to nothing");

    const uint8_t junk[] = {0xFF, 0xFF, 0xFF, 0xFF};
    dec.Decode(junk, out);
}

void TestCloseIsRepeatable() {
    std::printf("[audio] closing twice and reopening is safe...\n");
    if (BuiltWithoutOpus()) return;
    const AudioFormat format{};
    OpusAudioEncoder enc;
    Check(enc.Open(format, deskhub::media::kAudioBitrateBps), "first open");
    enc.Close();
    enc.Close();
    Check(!enc.IsOpen(), "a closed encoder says so");
    Check(enc.Open(format, deskhub::media::kAudioBitrateBps), "reopening works");
    Check(OpusAudioEncoder::BackendName() != nullptr, "the backend names itself for the logs");
}

void TestSinkPlaysOrDeclines() {
    std::printf("[audio] the sink either plays a decoded frame or refuses cleanly...\n");
    const AudioFormat format{};
    AudioSink sink;
    const std::vector<int16_t> frame = Tone(format, 0);

    if (!sink.Open(format)) {
        std::printf("[audio] no %s server here - checking the refusal path only\n",
            AudioSink::BackendName());
        Check(!sink.IsOpen(), "a sink that cannot open says so");
        Check(!sink.Write(frame), "a closed sink accepts nothing");
        Check(sink.framesQueued() == 0, "a closed sink queues nothing");
        return;
    }

    Check(sink.IsOpen(), "an opened sink says so");
    Check(!sink.Write(std::span<const int16_t>()), "an empty write is refused");
    std::vector<int16_t> halfFrame(format.interleavedSamples() / 2);
    Check(!sink.Write(halfFrame), "a partial frame is refused");

    for (int f = 0; f < 3; ++f) Check(sink.Write(Tone(format, f)), "whole frames are accepted");
    for (int waited = 0; waited < kPlayoutWaitTicks && sink.framesQueued() > 0; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Check(sink.framesQueued() == 0, "the audio device drained what was written to it");

    sink.Close();
    Check(!sink.IsOpen(), "a closed sink says so");
}

void TestPlayerRunsTheWholeReceivingSide() {
    std::printf(
        "[audio] a captured frame survives encode, the wire, the jitter buffer "
        "and playback...\n");
    const AudioFormat format{};
    if (BuiltWithoutOpus()) return;
    AudioPlayer player;
    if (!player.Start(format)) {
        std::printf("[audio] no audio device here - skipping the play-out path\n");
        Check(!player.running(), "a player that cannot start says so");
        return;
    }

    OpusAudioEncoder enc;
    Check(enc.Open(format, deskhub::media::kAudioBitrateBps), "the host-side encoder opens");

    std::vector<uint8_t> packet(kMaxOpusPacketBytes);
    uint8_t datagram[deskhub::kMaxDatagram];
    constexpr int kFrames = 10;
    constexpr uint32_t kLostFrame = 4;

    for (int f = 0; f < kFrames; ++f) {
        const size_t written = enc.Encode(Tone(format, f), packet);
        Check(written > 0, "the frame encodes");
        if (uint32_t(f) == kLostFrame) continue;

        const deskhub::AudioHeader ah{uint32_t(f), uint64_t(f) * deskhub::kAudioFrameMs * 1000};
        const size_t n = deskhub::BuildAudioPacket(datagram, 0x1234, ah,
            std::span<const uint8_t>(packet.data(), written));
        Check(n > 0, "and goes on the wire");
        const auto view = deskhub::ParseAudioPacket(deskhub::PayloadOf(
            std::span<const uint8_t>(datagram, n)));
        Check(view.has_value(), "and parses on the other side");
        player.Push(*view);
        std::this_thread::sleep_for(std::chrono::milliseconds(deskhub::kAudioFrameMs));
    }

    for (int waited = 0; waited < kPlayoutWaitTicks; ++waited) {
        if (player.stats().jitter.framesPlayed > 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const AudioPlayer::Stats stats = player.stats();
    Check(stats.jitter.framesReceived == kFrames - 1,
        "every frame sent arrived, the lost one aside");
    Check(stats.jitter.framesPlayed > 0, "the chain carried sound all the way to the speaker");
    Check(stats.decodeFailures == 0, "nothing failed to decode");

    player.Stop();
    Check(!player.running(), "the player stops cleanly");
}

}

void RunOpusCodecTests() {
    TestRoundTrip();
    TestConcealmentFillsAFrame();
    TestBadInputIsRefused();
    TestCloseIsRepeatable();
    TestSinkPlaysOrDeclines();
    TestPlayerRunsTheWholeReceivingSide();
}
