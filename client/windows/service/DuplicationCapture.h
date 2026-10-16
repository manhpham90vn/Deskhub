#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

#include "PointerCompositor.h"

class DuplicationCapture {
public:
    enum class Result { Frame,
        Unchanged,
        Lost };

    bool Open(const RECT& monitorRect);
    void Close();
    bool IsOpen() const;

    Result Next(UINT timeoutMs);

    uint32_t Width() const {
        return width_;
    }
    uint32_t Height() const {
        return height_;
    }
    const std::vector<uint8_t>& Pixels() const {
        return composed_;
    }

private:
    bool FindOutput(const RECT& monitorRect, Microsoft::WRL::ComPtr<IDXGIAdapter1>& adapter,
        Microsoft::WRL::ComPtr<IDXGIOutput1>& output);
    bool CopyDesktop(IDXGIResource* resource);
    bool ReadPointerShape(const DXGI_OUTDUPL_FRAME_INFO& info);
    void Compose();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> duplication_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_;
    PointerCompositor pointer_;
    std::vector<uint8_t> desktop_;
    std::vector<uint8_t> composed_;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
