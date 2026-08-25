#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "decode/PanelRenderer.h"

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <cstdio>
#include <mutex>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "gpu/D3D11VideoProcessor.h"
#include "gpu/HrCheck.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

#define PR_CHECK(expr, msg) DH_HR_CHECK("PanelRenderer", expr, msg)

struct PanelRenderer::Impl {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain1> swapchain;
    ComPtr<ID3D11Texture2D> backbuffer;
    D3D11VideoProcessor vp;
    std::mutex renderMutex;
    uint32_t bbW = 0, bbH = 0;
    uint32_t vpSrcW = 0, vpSrcH = 0;
    bool tearingAllowed = false;

    static bool TearingSupported(IDXGIFactory2* factory) {
        ComPtr<IDXGIFactory5> factory5;
        if (FAILED(factory->QueryInterface(IID_PPV_ARGS(&factory5)))) return false;
        BOOL allowed = FALSE;
        if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowed,
                sizeof(allowed))))
            return false;
        return allowed != FALSE;
    }

    UINT SwapChainFlags() const {
        return tearingAllowed ? UINT(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) : 0u;
    }

    UINT PresentFlags() const {
        return tearingAllowed ? UINT(DXGI_PRESENT_ALLOW_TEARING) : 0u;
    }

    bool Init(ID3D11Device* dev, HWND hwnd, uint32_t initialW, uint32_t initialH) {
        if (!hwnd) return false;
        device = dev;
        device->GetImmediateContext(&context);
        ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&mt)))) mt->SetMultithreadProtected(TRUE);

        bbW = initialW ? initialW : 16;
        bbH = initialH ? initialH : 16;

        ComPtr<IDXGIDevice> dxgiDev;
        PR_CHECK(device.As(&dxgiDev), "IDXGIDevice");
        ComPtr<IDXGIAdapter> adapter;
        PR_CHECK(dxgiDev->GetAdapter(&adapter), "GetAdapter");
        ComPtr<IDXGIFactory2> factory;
        PR_CHECK(adapter->GetParent(IID_PPV_ARGS(&factory)), "GetParent(Factory2)");

        {
            ComPtr<IDXGIDevice1> dxgiDev1;
            if (SUCCEEDED(dxgiDev.As(&dxgiDev1))) dxgiDev1->SetMaximumFrameLatency(1);
        }

        tearingAllowed = TearingSupported(factory.Get());

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = bbW;
        sd.Height = bbH;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        sd.Flags = SwapChainFlags();
        PR_CHECK(factory->CreateSwapChainForHwnd(device.Get(), hwnd, &sd, nullptr, nullptr,
                     &swapchain),
            "CreateSwapChainForHwnd");
        ComPtr<IDXGIFactory1> parent;
        if (SUCCEEDED(swapchain->GetParent(IID_PPV_ARGS(&parent))))
            parent->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        PR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)), "GetBuffer");
        return true;
    }

    bool Resize(uint32_t w, uint32_t h) {
        backbuffer.Reset();
        vp.Reset();
        PR_CHECK(swapchain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, SwapChainFlags()),
            "ResizeBuffers");
        PR_CHECK(swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer)), "GetBuffer(resize)");
        bbW = w;
        bbH = h;
        vpSrcW = vpSrcH = 0;
        return true;
    }

    bool EnsureVideoProcessor(uint32_t w, uint32_t h) {
        if (vp.ready() && w == vpSrcW && h == vpSrcH) return true;

        D3D11VideoProcessor::Setup s;
        s.srcWidth = w;
        s.srcHeight = h;
        s.dstWidth = bbW;
        s.dstHeight = bbH;
        s.srcRect = RECT{0, 0, (LONG)w, (LONG)h};
        s.dstRect = RECT{0, 0, (LONG)bbW, (LONG)bbH};
        s.srcYCbCr = true;
        if (!vp.Configure(device.Get(), backbuffer.Get(), s, "PanelRenderer")) return false;

        vpSrcW = w;
        vpSrcH = h;
        return true;
    }

    bool RenderNV12(ID3D11Texture2D* tex, UINT subresource, uint32_t w, uint32_t h,
        uint64_t* outReadyUs) {
        std::lock_guard<std::mutex> lk(renderMutex);
        if (!swapchain) return false;
        if ((w != bbW || h != bbH) && !Resize(w, h)) return false;
        if (!EnsureVideoProcessor(w, h)) return false;

        if (!vp.Blt(tex, subresource)) return false;
        if (outReadyUs) *outReadyUs = NowUs();
        PR_CHECK(swapchain->Present(0, PresentFlags()), "Present");
        return true;
    }
};

PanelRenderer::PanelRenderer() = default;
PanelRenderer::~PanelRenderer() = default;

bool PanelRenderer::InitForHwnd(ID3D11Device* device, void* hwnd, uint32_t initialW,
    uint32_t initialH) {
    impl_ = std::make_unique<Impl>();
    if (!impl_->Init(device, (HWND)hwnd, initialW, initialH)) {
        impl_.reset();
        return false;
    }
    return true;
}

bool PanelRenderer::RenderNV12(ID3D11Texture2D* tex, unsigned subresource, uint32_t width,
    uint32_t height, uint64_t* outReadyUs) {
    return impl_ && impl_->RenderNV12(tex, subresource, width, height, outReadyUs);
}
