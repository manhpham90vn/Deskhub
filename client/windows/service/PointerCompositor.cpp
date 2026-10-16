#include "PointerCompositor.h"

#include <utility>

namespace {

constexpr uint32_t kRgbMask = 0x00FFFFFFu;
constexpr uint32_t kOpaque = 0xFF000000u;

uint32_t LoadPixel(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

void StorePixel(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
    p[2] = uint8_t(v >> 16);
    p[3] = uint8_t(v >> 24);
}

uint8_t Blend(uint8_t src, uint8_t dst, uint8_t alpha) {
    return uint8_t((src * alpha + dst * (255 - alpha) + 127) / 255);
}

bool MaskBit(const uint8_t* row, uint32_t x) {
    return (row[x / 8] & (0x80 >> (x % 8))) != 0;
}

template <class Fn>
void ForEachCovered(POINT origin, uint32_t shapeW, uint32_t shapeH, uint32_t width,
    uint32_t height, Fn&& fn) {
    for (uint32_t sy = 0; sy < shapeH; ++sy) {
        const int64_t y = int64_t(origin.y) + sy;
        if (y < 0 || y >= int64_t(height)) continue;
        for (uint32_t sx = 0; sx < shapeW; ++sx) {
            const int64_t x = int64_t(origin.x) + sx;
            if (x < 0 || x >= int64_t(width)) continue;
            fn(uint32_t(x), uint32_t(y), sx, sy);
        }
    }
}

}

void PointerCompositor::UpdatePosition(const DXGI_OUTDUPL_POINTER_POSITION& position) {
    visible_ = position.Visible != FALSE;
    position_ = position.Position;
}

void PointerCompositor::UpdateShape(const DXGI_OUTDUPL_POINTER_SHAPE_INFO& info,
    std::vector<uint8_t> shape) {
    info_ = info;
    shape_ = std::move(shape);
}

void PointerCompositor::DrawOnto(std::span<uint8_t> bgra, uint32_t width, uint32_t height) const {
    if (!visible_ || shape_.empty() || bgra.size() < size_t(width) * height * 4) return;
    switch (info_.Type) {
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR: DrawColor(bgra, width, height); break;
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
            DrawMaskedColor(bgra, width, height);
            break;
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME: DrawMonochrome(bgra, width, height); break;
        default: break;
    }
}

void PointerCompositor::DrawColor(std::span<uint8_t> bgra, uint32_t width, uint32_t height) const {
    if (shape_.size() < size_t(info_.Pitch) * info_.Height) return;
    ForEachCovered(position_, info_.Width, info_.Height, width, height,
        [&](uint32_t x, uint32_t y, uint32_t sx, uint32_t sy) {
            const uint8_t* src = shape_.data() + size_t(sy) * info_.Pitch + size_t(sx) * 4;
            uint8_t* dst = bgra.data() + (size_t(y) * width + x) * 4;
            const uint8_t alpha = src[3];
            for (int c = 0; c < 3; ++c) dst[c] = Blend(src[c], dst[c], alpha);
            dst[3] = 0xFF;
        });
}

void PointerCompositor::DrawMaskedColor(std::span<uint8_t> bgra, uint32_t width,
    uint32_t height) const {
    if (shape_.size() < size_t(info_.Pitch) * info_.Height) return;
    ForEachCovered(position_, info_.Width, info_.Height, width, height,
        [&](uint32_t x, uint32_t y, uint32_t sx, uint32_t sy) {
            const uint32_t src = LoadPixel(shape_.data() + size_t(sy) * info_.Pitch + size_t(sx) * 4);
            uint8_t* dst = bgra.data() + (size_t(y) * width + x) * 4;
            const uint32_t screen = LoadPixel(dst);
            const uint32_t out = (src & kOpaque) ? ((screen ^ src) & kRgbMask) : (src & kRgbMask);
            StorePixel(dst, out | kOpaque);
        });
}

void PointerCompositor::DrawMonochrome(std::span<uint8_t> bgra, uint32_t width,
    uint32_t height) const {
    const uint32_t shapeH = info_.Height / 2;
    if (shape_.size() < size_t(info_.Pitch) * info_.Height) return;
    ForEachCovered(position_, info_.Width, shapeH, width, height,
        [&](uint32_t x, uint32_t y, uint32_t sx, uint32_t sy) {
            const uint8_t* andRow = shape_.data() + size_t(sy) * info_.Pitch;
            const uint8_t* xorRow = shape_.data() + size_t(sy + shapeH) * info_.Pitch;
            const uint32_t andMask = MaskBit(andRow, sx) ? 0xFFFFFFFFu : kOpaque;
            const uint32_t xorMask = MaskBit(xorRow, sx) ? kRgbMask : 0u;
            uint8_t* dst = bgra.data() + (size_t(y) * width + x) * 4;
            StorePixel(dst, ((LoadPixel(dst) & andMask) ^ xorMask) | kOpaque);
        });
}
