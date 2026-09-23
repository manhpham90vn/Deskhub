#include "deskhub/session/host/Beacon.h"
#include "deskhub/session/client/ScreenClientSession.h"
#include "deskhub/session/host/ScreenHostSession.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

using namespace deskhub;

namespace {

bool FillPattern(std::span<uint8_t> out) {
    uint8_t v = 0x5A;
    for (auto& b : out) b = ++v;
    return true;
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    ScreenHostCallbacks hostCb;
    hostCb.randomBytes = FillPattern;
    ScreenHostSession host(std::move(hostCb), StreamParams{1280, 720, 30, 8'000'000});

    ScreenClientSession client(ScreenClientSessionCallbacks{});
    client.Start(Hello{7, 1920, 1080, 0, 0}, 1);

    Beacon beacon;
    const SourceInfo sources[2] = {{0, 1280, 720, "Display 1"}, {1, 1920, 1080, "Display 2"}};
    beacon.SetSources(sources);

    uint8_t reply[kMaxDatagram];
    uint64_t now = 1'000'000;
    size_t pos = 0;
    while (pos < size) {
        const uint8_t op = data[pos++];
        const size_t len = std::min(size - pos, size_t(1) + size_t(op & 0x7F) * 12);
        const std::span<const uint8_t> pkt(data + pos, len);
        pos += len;
        const uint64_t from = (op & 0x80) ? 0x7F000001'1111ULL : 0x7F000001'2222ULL;
        host.HandlePacket(pkt, now, from);
        client.HandlePacket(pkt, now);
        beacon.Reply(reply, pkt);
        now += (op & 0x0F) == 0 ? kSessionTimeoutUs + 1 : 20'000;
        host.Tick(now);
        client.Tick(now);
    }
    return 0;
}
