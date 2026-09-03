#pragma once
#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

inline constexpr uint64_t kTestViewer = 0xC0A80001'0000ULL;
inline constexpr const char* kTestPasscode = "0417";

extern int g_failures;
void Check(bool ok, const char* what);

uint32_t Rnd();

bool TestRandomBytes(std::span<uint8_t> out);

struct TestFrame {
    uint32_t id;
    bool idr;
    std::vector<uint8_t> nal;
};

using Datagram = std::vector<uint8_t>;

std::vector<TestFrame> MakeFrames(size_t count, size_t gop);

TestFrame MakeIdrFrame(uint32_t id, size_t pkts);

std::vector<Datagram> Packetize(deskhub::Packetizer& pk, const TestFrame& f, uint64_t tsUs);

void Feed(deskhub::Reassembler& ra, const Datagram& d, uint64_t nowUs);

bool IsFec(const Datagram& d);

size_t NthDataPacket(const std::vector<Datagram>& pkts, size_t n);

bool SameFrame(const deskhub::Reassembler::Frame& got, const TestFrame& want);

class SeededRng {
public:
    explicit SeededRng(uint64_t seed) : state_(seed ? seed : 0x9E3779B97F4A7C15ull) {}

    uint64_t Next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 7;
        state_ ^= state_ << 17;
        return state_;
    }

    double Unit() {
        return double(Next() >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    uint64_t state_;
};

enum class LossKind { Uniform,
    GilbertElliott };

class LossSource {
public:
    LossSource(LossKind kind, double lossRate, double meanBurstPackets, uint64_t seed)
        : kind_(kind), rng_(seed), lossRate_(lossRate) {
        if (meanBurstPackets < 1.0) meanBurstPackets = 1.0;
        toGood_ = 1.0 / meanBurstPackets;
        toBad_ = lossRate < 1.0 ? lossRate * toGood_ / (1.0 - lossRate) : 1.0;
    }

    bool Drop() {
        if (kind_ == LossKind::Uniform) return rng_.Unit() < lossRate_;
        if (bad_) {
            if (rng_.Unit() < toGood_) bad_ = false;
        } else if (rng_.Unit() < toBad_) {
            bad_ = true;
        }
        return bad_;
    }

private:
    LossKind kind_;
    SeededRng rng_;
    double lossRate_;
    double toGood_ = 1.0;
    double toBad_ = 0.0;
    bool bad_ = false;
};

std::span<const std::string_view> FecSchemesUnderTest();

bool SchemeIsUnderTest(std::string_view name);
