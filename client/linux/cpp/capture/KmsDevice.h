#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "deskhub/media/ShareSource.h"

inline constexpr uint32_t kMaxKmsPlanes = 4;

class KmsBuffer {
public:
    KmsBuffer() = default;
    ~KmsBuffer();
    KmsBuffer(KmsBuffer&& other) noexcept;
    KmsBuffer& operator=(KmsBuffer&& other) noexcept;
    KmsBuffer(const KmsBuffer&) = delete;
    KmsBuffer& operator=(const KmsBuffer&) = delete;

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
    uint64_t modifier = 0;
    uint32_t planeCount = 0;
    int fds[kMaxKmsPlanes] = {-1, -1, -1, -1};
    uint32_t offsets[kMaxKmsPlanes]{};
    uint32_t pitches[kMaxKmsPlanes]{};
    uint64_t identity = 0;

private:
    void CloseAll();
};

struct KmsCursor {
    uint32_t fbId = 0;
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t srcWidth = 0;
    uint32_t srcHeight = 0;
};

struct KmsScanout {
    uint32_t fbId = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    std::optional<KmsCursor> cursor;
};

class KmsCard {
public:
    static std::optional<KmsCard> Open(uint32_t cardIndex);

    KmsCard() = default;
    ~KmsCard();
    KmsCard(KmsCard&& other) noexcept;
    KmsCard& operator=(KmsCard&& other) noexcept;
    KmsCard(const KmsCard&) = delete;
    KmsCard& operator=(const KmsCard&) = delete;

    std::optional<KmsScanout> Scanout(uint32_t crtcId);
    std::optional<KmsBuffer> Export(uint32_t fbId) const;
    std::string RenderNode() const;
    std::vector<deskhub::media::ShareSource> Displays(uint32_t cardIndex) const;

private:
    explicit KmsCard(int fd) : fd_(fd) {}

    std::optional<KmsCursor> CursorOn(uint32_t crtcId);
    std::optional<uint32_t> FindCursorPlane(uint32_t crtcId) const;

    int fd_ = -1;
    uint32_t cursorCrtc_ = 0;
    std::optional<uint32_t> cursorPlane_;
};

uint64_t KmsTargetId(uint32_t cardIndex, uint32_t crtcId);
uint32_t KmsCardOf(uint64_t targetId);
uint32_t KmsCrtcOf(uint64_t targetId);

std::vector<deskhub::media::ShareSource> ListKmsDisplays();
