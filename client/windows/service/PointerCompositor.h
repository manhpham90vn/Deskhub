#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <dxgi1_2.h>

#include <cstdint>
#include <span>
#include <vector>

class PointerCompositor {
public:
    void UpdatePosition(const DXGI_OUTDUPL_POINTER_POSITION& position);
    void UpdateShape(const DXGI_OUTDUPL_POINTER_SHAPE_INFO& info, std::vector<uint8_t> shape);
    void DrawOnto(std::span<uint8_t> bgra, uint32_t width, uint32_t height) const;

private:
    void DrawColor(std::span<uint8_t> bgra, uint32_t width, uint32_t height) const;
    void DrawMaskedColor(std::span<uint8_t> bgra, uint32_t width, uint32_t height) const;
    void DrawMonochrome(std::span<uint8_t> bgra, uint32_t width, uint32_t height) const;

    DXGI_OUTDUPL_POINTER_SHAPE_INFO info_{};
    std::vector<uint8_t> shape_;
    POINT position_{};
    bool visible_ = false;
};
