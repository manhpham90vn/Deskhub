#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <cstdint>

#include "gpu/D3D11VideoProcessor.h"

class Downscaler {
public:
    Downscaler() = default;
    ~Downscaler() = default;
    Downscaler(const Downscaler&) = delete;
    Downscaler& operator=(const Downscaler&) = delete;

    bool Configure(ID3D11Device* device, uint32_t srcW, uint32_t srcH, uint32_t dstW,
        uint32_t dstH);

    ID3D11Texture2D* Scale(ID3D11Texture2D* src);

private:
    void Reset();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> dstTex_;
    D3D11VideoProcessor vp_;

    uint32_t srcW_ = 0, srcH_ = 0, dstW_ = 0, dstH_ = 0;
};
