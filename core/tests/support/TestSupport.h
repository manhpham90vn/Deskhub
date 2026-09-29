#pragma once
#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"

#include <cstddef>
#include <cstdint>
#include <vector>

inline constexpr uint64_t kTestViewer = 0xC0A80001'0000ULL;

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
