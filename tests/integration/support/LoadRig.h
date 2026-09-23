#pragma once
#include "support/FakeDecoder.h"
#include "support/TestSupport.h"

#include "deskhubp/client/FileTransferClient.h"
#include "deskhubp/client/ScreenViewer.h"
#include "deskhubp/system/Clock.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace load {

inline std::filesystem::path Scratch(const std::string& leaf) {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "deskhub-load-tests";
    std::error_code ec;
    std::filesystem::remove_all(root / leaf, ec);
    std::filesystem::create_directories(root / leaf, ec);
    return root / leaf;
}

inline std::vector<uint8_t> Pattern(size_t size) {
    std::vector<uint8_t> bytes(size);
    for (size_t i = 0; i < size; ++i) bytes[i] = uint8_t(i * 131u + (i >> 13));
    return bytes;
}

inline std::filesystem::path WriteBytes(const std::filesystem::path& dir, const std::string& name,
    const std::vector<uint8_t>& bytes) {
    const std::filesystem::path path = dir / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    return path;
}

inline std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

struct Upload {
    deskhubp::FileTransferClient client{};

    ~Upload() {
        client.Stop();
    }

    bool Start(uint16_t port, const std::filesystem::path& file) {
        deskhubp::FileTransferClientConfig config;
        config.host = NetAddr{0x7F000001u, port};
        config.hostLabel = config.host.ToString();
        config.passcode = kTestPasscode;
        config.clientName = "load-test-sender";
        config.files = {file};
        return client.Start(config, deskhubp::FileTransferClientCallbacks{});
    }

    bool busy() const {
        return !client.Finished();
    }

    bool done() const {
        return client.State() == deskhubp::FileTransferClientState::Done;
    }

    uint64_t batchBytes() const {
        return client.Progress().batchBytes;
    }
};

inline uint64_t DecodedFrames() {
    return uint64_t(fake::Decoded().frameCount());
}

inline deskhubp::ScreenViewerConfig ViewerConfig(uint16_t port, bool wantsAudio) {
    deskhubp::ScreenViewerConfig cfg;
    cfg.server = NetAddr{0x7F000001u, port};
    cfg.sourceId = 0;
    cfg.screenW = 1920;
    cfg.screenH = 1080;
    cfg.alwaysFocused = true;
    cfg.passcode = kTestPasscode;
    cfg.wantsAudio = wantsAudio;
    return cfg;
}

class Continuity {
public:
    explicit Continuity(std::function<uint64_t()> count) : count_(std::move(count)) {}

    void Begin() {
        first_ = count_();
        last_ = first_;
        startUs_ = NowUs();
        lastMoveUs_ = startUs_;
        worstGapUs_ = 0;
    }

    void Sample() {
        const uint64_t seen = count_();
        const uint64_t now = NowUs();
        if (seen != last_) {
            last_ = seen;
            lastMoveUs_ = now;
            return;
        }
        if (now - lastMoveUs_ > worstGapUs_) worstGapUs_ = now - lastMoveUs_;
    }

    uint64_t moved() const {
        return last_ - first_;
    }

    uint64_t worstGapMs() const {
        return worstGapUs_ / 1000;
    }

    double perSecond() const {
        const uint64_t spentUs = NowUs() - startUs_;
        return spentUs == 0 ? 0.0 : double(moved()) * 1'000'000.0 / double(spentUs);
    }

private:
    std::function<uint64_t()> count_;
    uint64_t first_ = 0;
    uint64_t last_ = 0;
    uint64_t startUs_ = 0;
    uint64_t lastMoveUs_ = 0;
    uint64_t worstGapUs_ = 0;
};

}
