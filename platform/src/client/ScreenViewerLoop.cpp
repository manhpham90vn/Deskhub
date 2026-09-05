#include "deskhubp/client/ScreenViewerLoop.h"

#include "deskhubp/system/Clock.h"

#if defined(_WIN32)
#include <processthreadsapi.h>
#else
#include <unistd.h>
#endif

namespace deskhubp {

namespace {

uint32_t CurrentProcessId() {
#if defined(_WIN32)
    return uint32_t(GetCurrentProcessId());
#else
    return uint32_t(getpid());
#endif
}

}

uint32_t MakeClientId(uint8_t sourceId) {
    return uint32_t(NowUs()) ^ CurrentProcessId() ^ (uint32_t(sourceId) << 24);
}

void RunScreenViewerLoop(const ScreenViewerRecv& recv, deskhub::ScreenClient& screen,
    const ScreenViewerLoopHooks& hooks) {
    uint8_t buf[deskhub::kMaxRecordSize];

    for (;;) {
        if (hooks.stopped && hooks.stopped()) break;

        const int n = recv(buf, sizeof(buf));
        const uint64_t now = NowUs();
        if (n < 0) {
            if (hooks.onSocketError) hooks.onSocketError();
            break;
        }
        if (n > 0) {
            const std::span<const uint8_t> message(buf, size_t(n));
            const auto header = deskhub::ParseCommonHeader(message);
            if (header && header->chan == deskhub::Chan::File) {
                if (hooks.onFile) hooks.onFile(message);
            } else {
                screen.OnDatagram(message, now);
            }
        }
        if (hooks.pumpFiles) hooks.pumpFiles();

        screen.PollFrames(now);
        if (hooks.afterFrames) hooks.afterFrames(screen, now);

        screen.PlanNacks(now);
        if (hooks.beforeTick) hooks.beforeTick(screen, now);

        if (!screen.Tick(now) && !(hooks.onSessionDead && hooks.onSessionDead())) break;
        if (hooks.onPhase) hooks.onPhase(screen.streaming());

        screen.CountLoopBusy(now, NowUs());
    }

    screen.SendBye();
}

}
