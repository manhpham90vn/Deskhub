#include "DuplicationCapture.h"

#include <cstring>

#include "deskhubp/diag/Log.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace {

bool SameRect(const RECT& a, const RECT& b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

}

bool DuplicationCapture::FindOutput(const RECT& monitorRect, ComPtr<IDXGIAdapter1>& adapter,
    ComPtr<IDXGIOutput1>& output) {
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return false;
    for (UINT a = 0; factory->EnumAdapters1(a, &adapter) != DXGI_ERROR_NOT_FOUND; ++a) {
        ComPtr<IDXGIOutput> candidate;
        for (UINT o = 0; adapter->EnumOutputs(o, &candidate) != DXGI_ERROR_NOT_FOUND; ++o) {
            DXGI_OUTPUT_DESC desc{};
            if (SUCCEEDED(candidate->GetDesc(&desc)) && desc.AttachedToDesktop &&
                SameRect(desc.DesktopCoordinates, monitorRect) &&
                SUCCEEDED(candidate.As(&output)))
                return true;
            candidate.Reset();
        }
        adapter.Reset();
    }
    return false;
}

bool DuplicationCapture::Open(const RECT& monitorRect) {
    Close();
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput1> output;
    if (!FindOutput(monitorRect, adapter, output)) {
        LOGW("[Agent] No display output at (%ld,%ld)-(%ld,%ld).", monitorRect.left,
            monitorRect.top, monitorRect.right, monitorRect.bottom);
        return false;
    }
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};
    HRESULT hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &device_,
        nullptr, &context_);
    if (FAILED(hr)) {
        LOGE("[Agent] D3D11CreateDevice failed: 0x%08lX", (unsigned long)hr);
        return false;
    }
    hr = output->DuplicateOutput(device_.Get(), &duplication_);
    if (FAILED(hr)) {
        LOGW("[Agent] DuplicateOutput failed: 0x%08lX", (unsigned long)hr);
        Close();
        return false;
    }
    DXGI_OUTDUPL_DESC desc{};
    duplication_->GetDesc(&desc);
    width_ = desc.ModeDesc.Width;
    height_ = desc.ModeDesc.Height;
    desktop_.clear();
    return true;
}

void DuplicationCapture::Close() {
    duplication_.Reset();
    staging_.Reset();
    context_.Reset();
    device_.Reset();
}

bool DuplicationCapture::IsOpen() const {
    return duplication_ != nullptr;
}

bool DuplicationCapture::CopyDesktop(IDXGIResource* resource) {
    ComPtr<ID3D11Texture2D> frame;
    if (FAILED(resource->QueryInterface(IID_PPV_ARGS(&frame)))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    frame->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        LOGW("[Agent] Unsupported desktop format %d.", int(desc.Format));
        return false;
    }
    D3D11_TEXTURE2D_DESC have{};
    if (staging_) staging_->GetDesc(&have);
    if (!staging_ || have.Width != desc.Width || have.Height != desc.Height) {
        staging_.Reset();
        D3D11_TEXTURE2D_DESC sd = desc;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.BindFlags = 0;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.MiscFlags = 0;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        sd.SampleDesc = {1, 0};
        if (FAILED(device_->CreateTexture2D(&sd, nullptr, &staging_))) return false;
    }
    context_->CopyResource(staging_.Get(), frame.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
    width_ = desc.Width;
    height_ = desc.Height;
    const size_t rowBytes = size_t(width_) * 4;
    desktop_.resize(rowBytes * height_);
    for (uint32_t y = 0; y < height_; ++y)
        std::memcpy(desktop_.data() + y * rowBytes,
            static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, rowBytes);
    context_->Unmap(staging_.Get(), 0);
    return true;
}

bool DuplicationCapture::ReadPointerShape(const DXGI_OUTDUPL_FRAME_INFO& info) {
    std::vector<uint8_t> shape(info.PointerShapeBufferSize);
    UINT needed = 0;
    DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo{};
    if (FAILED(duplication_->GetFramePointerShape(UINT(shape.size()), shape.data(), &needed,
            &shapeInfo)))
        return false;
    shape.resize(needed);
    pointer_.UpdateShape(shapeInfo, std::move(shape));
    return true;
}

void DuplicationCapture::Compose() {
    composed_ = desktop_;
    pointer_.DrawOnto(composed_, width_, height_);
}

DuplicationCapture::Result DuplicationCapture::Next(UINT timeoutMs) {
    if (!duplication_) return Result::Lost;
    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;
    const HRESULT hr = duplication_->AcquireNextFrame(timeoutMs, &info, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) return Result::Unchanged;
    if (FAILED(hr)) {
        Close();
        return Result::Lost;
    }
    bool changed = false;
    if (info.LastPresentTime.QuadPart != 0) changed |= CopyDesktop(resource.Get());
    if (info.LastMouseUpdateTime.QuadPart != 0) {
        pointer_.UpdatePosition(info.PointerPosition);
        changed = true;
    }
    if (info.PointerShapeBufferSize > 0) changed |= ReadPointerShape(info);
    duplication_->ReleaseFrame();
    if (!changed || desktop_.empty()) return Result::Unchanged;
    Compose();
    return Result::Frame;
}
