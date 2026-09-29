#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/ShareFlow.h"

#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

void TestClampKeepsTheFirstSources() {
    std::printf("[share] more displays than the protocol carries get clamped, with a note...\n");
    std::vector<media::ShareSource> many;
    for (uint64_t i = 0; i < kMaxSources + 3; ++i) {
        media::ShareSource s;
        s.targetId = i + 1;
        many.push_back(s);
    }

    const ShareClampResult clamped = ClampShareSources(many);
    Check(clamped.clamped, "the caller is told to warn the user");
    Check(clamped.sources.size() == kMaxSources, "only the protocol maximum survives");
    Check(clamped.sources.front().targetId == 1 && clamped.sources.back().targetId == kMaxSources,
        "and it is the first N displays, in order");

    const ShareClampResult fits = ClampShareSources({many.begin(), many.begin() + 2});
    Check(!fits.clamped && fits.sources.size() == 2, "a small list passes through untouched");
}

void TestSettingsBecomeShareOptions() {
    std::printf("[share] the settings every client stores become the options it hosts with...\n");
    ui::UiSettings settings;
    settings.fps = 45;
    settings.bitrateMbps = 35;
    settings.maxDim = 2560;
    settings.port = 47999;
    settings.allowInput = false;
    settings.bindIp = "192.168.1.10";
    settings.deviceName = "study pc";
    settings.clipboardSync = true;
    settings.shareAudio = false;

    const media::ShareOptions options = ShareOptionsOf(settings, true);
    Check(options.fps == 45 && options.bitrateMbps == 35 && options.maxDim == 2560,
        "the picture settings carry over");
    Check(options.port == 47999, "so does the port");
    Check(!options.allowInput, "and the view-only switch");
    Check(options.bindIp == "192.168.1.10", "and the network");
    Check(options.deviceName == "study pc", "and the name viewers see");
    Check(options.clipboardSync && !options.audio, "and the clipboard and sound switches");
    Check(options.terminal, "the shell is asked for separately, not read from the file");
    Check(!ShareOptionsOf(settings, false).terminal, "and can be left out");
}

}

void RunShareFlowTests() {
    TestClampKeepsTheFirstSources();
    TestSettingsBecomeShareOptions();
}
