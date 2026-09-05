#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "encode/MfEncoder.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <codecapi.h>
#include <icodecapi.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdio>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "deskhub/media/AnnexB.h"
#include "deskhub/media/H264Sps.h"
#include "deskhub/media/RatePlan.h"
#include "deskhubp/diag/Log.h"
#include "gpu/D3D11VideoProcessor.h"
#include "gpu/HrCheck.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

#define MF_CHECK(expr, msg) DH_HR_CHECK("MfEncoder", expr, msg)
#define MF_CHECKI(expr, msg) DH_HR_CHECK_VAL("MfEncoder", expr, msg, -1)

struct MfEncoder::Impl {
    static constexpr uint32_t kLtrRingSlots = 4;

    struct LtrSlot {
        uint32_t frameId = 0;
        bool valid = false;
    };

    ComPtr<IMFActivate> activate;
    ComPtr<IMFTransform> mft;
    ComPtr<IMFMediaEventGenerator> events;
    ComPtr<IMFDXGIDeviceManager> deviceManager;
    ComPtr<ICodecAPI> codecApi;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11Texture2D> nv12Tex;
    D3D11VideoProcessor colorConvert;

    EncoderConfig cfg{};
    deskhub::media::EncoderRecoveryCaps recovery{};
    UINT resetToken = 0;
    bool mfStarted = false;
    bool streaming = false;
    bool isAsync = false;
    bool outputProvidesSamples = false;
    bool haveFirstTs = false;
    uint64_t firstTsUs = 0;
    int needInputCredit = 0;
    ULONGLONG lastEncodeTickMs = 0;
    bool rcLogged = false;
    uint64_t frameCount = 0;
    uint64_t totalBytes = 0;
    FILE* out = nullptr;
    std::vector<uint8_t> spsPps;

    uint32_t ltrSlots = 0;
    uint32_t nextLtrSlot = 0;
    LtrSlot ltrSlot[kLtrRingSlots]{};

    ~Impl() {
        if (mft && streaming) {
            mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
            mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
        }
        mft.Reset();
        events.Reset();
        codecApi.Reset();
        if (activate) activate->ShutdownObject();
        deviceManager.Reset();
        if (out) {
            std::fclose(out);
            out = nullptr;
        }
        if (mfStarted) MFShutdown();
    }

    struct AdapterId {
        LUID luid{};
        std::wstring description;
        bool known = false;
    };

    AdapterId DeviceAdapter() {
        AdapterId id;
        ComPtr<IDXGIDevice> dxgi;
        if (FAILED(device.As(&dxgi))) return id;
        ComPtr<IDXGIAdapter> adapter;
        if (FAILED(dxgi->GetAdapter(&adapter))) return id;
        DXGI_ADAPTER_DESC desc{};
        if (FAILED(adapter->GetDesc(&desc))) return id;
        id.luid = desc.AdapterLuid;
        id.description = desc.Description;
        id.known = true;
        return id;
    }

    static std::wstring MftName(IMFActivate* act) {
        wchar_t name[256] = L"?";
        UINT32 nameLen = 0;
        act->GetString(MFT_FRIENDLY_NAME_Attribute, name, 256, &nameLen);
        return std::wstring(name);
    }

    bool TakeFirstD3D11Aware(IMFActivate** activates, UINT32 count, const wchar_t* scope) {
        for (UINT32 i = 0; i < count && !activate; ++i) {
            const std::wstring name = MftName(activates[i]);

            ComPtr<IMFTransform> candidate;
            ComPtr<IMFAttributes> candidateAttrs;
            if (FAILED(activates[i]->ActivateObject(IID_PPV_ARGS(&candidate))) ||
                FAILED(candidate->GetAttributes(&candidateAttrs))) {
                std::wprintf(L"[MfEncoder] Found MFT: %ls (activate failed)\n", name.c_str());
                continue;
            }
            UINT32 aware = 0;
            candidateAttrs->GetUINT32(MF_SA_D3D11_AWARE, &aware);
            std::wprintf(L"[MfEncoder] Found MFT: %ls (%ls, D3D11-aware=%u)\n", name.c_str(),
                scope, aware);
            if (!aware) {
                activates[i]->ShutdownObject();
                continue;
            }
            activate = activates[i];
            mft = candidate;
        }
        return activate != nullptr;
    }

    bool TakeFromAdapter(const AdapterId& want, const MFT_REGISTER_TYPE_INFO& outInfo,
        UINT32 flags) {
        ComPtr<IMFAttributes> scoped;
        if (FAILED(MFCreateAttributes(&scoped, 1))) return false;
        if (FAILED(scoped->SetBlob(MFT_ENUM_ADAPTER_LUID,
                reinterpret_cast<const UINT8*>(&want.luid), sizeof(want.luid))))
            return false;

        IMFActivate** activates = nullptr;
        UINT32 count = 0;
        if (FAILED(MFTEnum2(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &outInfo, scoped.Get(),
                &activates, &count)))
            return false;

        const bool took =
            count > 0 && TakeFirstD3D11Aware(activates, count, want.description.c_str());
        for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
        CoTaskMemFree(activates);
        return took;
    }

    bool FindActivate() {
        MFT_REGISTER_TYPE_INFO outInfo{MFMediaType_Video, MFVideoFormat_H264};
        const UINT32 flags = MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER;

        const AdapterId want = DeviceAdapter();
        if (want.known && TakeFromAdapter(want, outInfo, flags)) return true;

        if (want.known)
            std::wprintf(
                L"[MfEncoder] No hardware encoder MFT is registered for %ls; falling "
                L"back to every hardware encoder this machine has.\n",
                want.description.c_str());
        else
            LOGW(
                "[MfEncoder] Could not read the adapter behind this device, so encoders cannot "
                "be scoped to it; falling back to every hardware encoder this machine has.");

        IMFActivate** activates = nullptr;
        UINT32 count = 0;
        MF_CHECK(MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, flags, nullptr, &outInfo, &activates,
                     &count),
            "MFTEnumEx");
        if (count == 0) {
            CoTaskMemFree(activates);
            LOGE("[MfEncoder] No encoder MFT found.");
            return false;
        }

        const bool took = TakeFirstD3D11Aware(activates, count, L"adapter not checked");
        for (UINT32 i = 0; i < count; ++i) activates[i]->Release();
        CoTaskMemFree(activates);

        if (!took) {
            LOGE("[MfEncoder] No D3D11-aware encoder MFT available.");
            return false;
        }
        if (want.known)
            LOGW(
                "[MfEncoder] This MFT was not scoped to the device's adapter. If it was built "
                "for a different one it will reject this device's textures, which is what "
                "MF_E_UNSUPPORTED_D3D_TYPE means when SetOutputType reports it.");
        return true;
    }

    bool ConfigureTransform() {
        ComPtr<IMFAttributes> mftAttrs;
        MF_CHECK(mft->GetAttributes(&mftAttrs), "GetAttributes");

        UINT32 asyncFlag = 0;
        mftAttrs->GetUINT32(MF_TRANSFORM_ASYNC, &asyncFlag);
        isAsync = asyncFlag != 0;
        if (isAsync) {
            MF_CHECK(mftAttrs->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE), "ASYNC_UNLOCK");
            MF_CHECK(mft.As(&events), "IMFMediaEventGenerator");
        }

        MF_CHECK(mft->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)deviceManager.Get()),
            "SET_D3D_MANAGER");

        ComPtr<IMFMediaType> outType;
        MF_CHECK(MFCreateMediaType(&outType), "MFCreateMediaType(out)");
        outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
        outType->SetUINT32(MF_MT_AVG_BITRATE, cfg.bitrateBps);
        outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        outType->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
        MF_CHECK(MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, cfg.width, cfg.height),
            "FRAME_SIZE(out)");
        MF_CHECK(MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, cfg.fps, 1),
            "FRAME_RATE(out)");
        MF_CHECK(MFSetAttributeRatio(outType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            "PAR(out)");
        MF_CHECK(mft->SetOutputType(0, outType.Get(), 0), "SetOutputType");

        ComPtr<IMFMediaType> inType;
        MF_CHECK(MFCreateMediaType(&inType), "MFCreateMediaType(in)");
        inType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        inType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        inType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        inType->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_16_235);
        inType->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
        MF_CHECK(MFSetAttributeSize(inType.Get(), MF_MT_FRAME_SIZE, cfg.width, cfg.height),
            "FRAME_SIZE(in)");
        MF_CHECK(MFSetAttributeRatio(inType.Get(), MF_MT_FRAME_RATE, cfg.fps, 1),
            "FRAME_RATE(in)");
        MF_CHECK(mft->SetInputType(0, inType.Get(), 0), "SetInputType");

        if (!SetupRateControl()) return false;

        MFT_OUTPUT_STREAM_INFO si{};
        MF_CHECK(mft->GetOutputStreamInfo(0, &si), "GetOutputStreamInfo");
        outputProvidesSamples = (si.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
                                                  MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;

        MF_CHECK(mft->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0),
            "NOTIFY_BEGIN_STREAMING");
        MF_CHECK(mft->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0),
            "NOTIFY_START_OF_STREAM");
        streaming = true;
        return true;
    }

    bool ReinitTransform() {
        if (mft && streaming) {
            mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
            mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
        }
        mft.Reset();
        events.Reset();
        codecApi.Reset();
        streaming = false;
        needInputCredit = 0;
        lastEncodeTickMs = 0;
        if (!activate) return false;
        activate->ShutdownObject();
        if (FAILED(activate->ActivateObject(IID_PPV_ARGS(&mft)))) {
            LOGE(
                "[MfEncoder] Failed to recreate the encoder - this MFT cannot retune "
                "live, so bitrate, frame rate and keyframe requests all rebuild it.");
            return false;
        }
        spsPps.clear();
        return ConfigureTransform();
    }

    bool Init(ID3D11Device* dev, const EncoderConfig& c) {
        cfg = c;
        device = dev;

        ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&mt)))) {
            mt->SetMultithreadProtected(TRUE);
        }

        MF_CHECK(MFStartup(MF_VERSION, MFSTARTUP_LITE), "MFStartup");
        mfStarted = true;

        if (!FindActivate()) return false;

        MF_CHECK(MFCreateDXGIDeviceManager(&resetToken, &deviceManager),
            "MFCreateDXGIDeviceManager");
        MF_CHECK(deviceManager->ResetDevice(device.Get(), resetToken), "ResetDevice");

        if (!ConfigureTransform()) return false;
        if (!SetupColorConvert()) return false;

        if (!OpenEncoderOutput(cfg, "MfEncoder", out)) return false;

        recovery = QueryRecoveryCaps();
        ConfigureLongTermReferences();

        LOGI("[MfEncoder] Initialized: %ux%u @%ufps, %.1f Mbps, H264%s -> %s",
            cfg.width, cfg.height, cfg.fps, cfg.bitrateBps / 1e6,
            isAsync ? " (async MFT)" : " (sync MFT)",
            out ? "file" : "callback");
        return true;
    }

    deskhub::media::EncoderRecoveryCaps QueryRecoveryCaps() {
        if (!codecApi) return {};
        auto supported = [&](const GUID& api, const char* name) {
            const HRESULT hr = codecApi->IsSupported(&api);
            LOGI("[MfEncoder] codecapi %s: IsSupported hr=0x%08lX", name, (unsigned long)hr);
            return hr == S_OK;
        };
        const bool ltrBuffers = supported(CODECAPI_AVEncVideoLTRBufferControl, "LTRBufferControl");
        const bool ltrMark = supported(CODECAPI_AVEncVideoMarkLTRFrame, "MarkLTRFrame");
        const bool ltrUse = supported(CODECAPI_AVEncVideoUseLTRFrame, "UseLTRFrame");
        const bool refresh =
            supported(CODECAPI_AVEncVideoGradualIntraRefresh, "GradualIntraRefresh");
        return {ltrBuffers && ltrMark && ltrUse, refresh};
    }

    bool SetCodecUI4(const GUID& api, ULONG value) {
        if (!codecApi || codecApi->IsSupported(&api) != S_OK) return false;
        VARIANT v{};
        v.vt = VT_UI4;
        v.ulVal = value;
        return SUCCEEDED(codecApi->SetValue(&api, &v));
    }

    void ForgetLongTermReferences() {
        nextLtrSlot = 0;
        for (LtrSlot& slot : ltrSlot) slot = {};
    }

    void ConfigureLongTermReferences() {
        ForgetLongTermReferences();
        ltrSlots = 0;
        if (!recovery.longTermReference) return;
        if (!SetCodecUI4(CODECAPI_AVEncVideoLTRBufferControl, kLtrRingSlots)) {
            LOGW(
                "[MfEncoder] The MFT would not reserve %u long-term reference buffers, so loss "
                "recovery falls back to IDR.",
                kLtrRingSlots);
            recovery.longTermReference = false;
            return;
        }
        ltrSlots = kLtrRingSlots;
    }

    bool MarkLongTermReference(uint32_t frameId) {
        if (!ltrSlots) return false;
        const uint32_t slot = nextLtrSlot;
        if (!SetCodecUI4(CODECAPI_AVEncVideoMarkLTRFrame, slot)) return false;
        nextLtrSlot = (nextLtrSlot + 1) % ltrSlots;
        ltrSlot[slot] = {frameId, true};
        return true;
    }

    bool InvalidateReference(uint32_t firstInvalidFrameId) {
        if (!ltrSlots) return false;
        uint32_t best = ltrSlots;
        for (uint32_t i = 0; i < ltrSlots; ++i) {
            if (!ltrSlot[i].valid) continue;
            if (ltrSlot[i].frameId >= firstInvalidFrameId) {
                ltrSlot[i].valid = false;
                continue;
            }
            if (best == ltrSlots || ltrSlot[i].frameId > ltrSlot[best].frameId) best = i;
        }
        if (best == ltrSlots) return false;
        if (!SetCodecUI4(CODECAPI_AVEncVideoUseLTRFrame, 1u << best)) return false;
        LOGI("[MfEncoder] recovery: next picture references long-term frame %u only.",
            ltrSlot[best].frameId);
        return true;
    }

    bool BeginIntraRefresh(uint32_t frames) {
        if (!recovery.intraRefresh || !frames) return false;
        if (!SetCodecUI4(CODECAPI_AVEncVideoGradualIntraRefresh, frames)) return false;
        LOGI("[MfEncoder] recovery: intra refresh over the next %u frames.", frames);
        return true;
    }

    bool SetupRateControl() {
        if (FAILED(mft.As(&codecApi))) {
            LOGW("[MfEncoder] Failed to get ICodecAPI - using MFT default parameters.");
            return true;
        }
        const bool log = !rcLogged;
        auto report = [&](const char* name, const char* what) {
            if (log) LOGW("[MfEncoder] codecapi %s: %s", name, what);
        };
        auto setUI4 = [&](const GUID& api, ULONG val, const char* name) {
            if (codecApi->IsSupported(&api) != S_OK) {
                report(name, "NOT SUPPORTED");
                return;
            }
            VARIANT v{};
            v.vt = VT_UI4;
            v.ulVal = val;
            report(name, SUCCEEDED(codecApi->SetValue(&api, &v)) ? "ok" : "SetValue FAILED");
        };
        auto setBool = [&](const GUID& api, bool val, const char* name) {
            if (codecApi->IsSupported(&api) != S_OK) {
                report(name, "NOT SUPPORTED");
                return;
            }
            VARIANT v{};
            v.vt = VT_BOOL;
            v.boolVal = val ? VARIANT_TRUE : VARIANT_FALSE;
            report(name, SUCCEEDED(codecApi->SetValue(&api, &v)) ? "ok" : "SetValue FAILED");
        };
        const bool vbr = cfg.rc == RateControl::VBR;
        setUI4(CODECAPI_AVEncCommonRateControlMode,
            (ULONG)(vbr ? eAVEncCommonRateControlMode_PeakConstrainedVBR
                        : eAVEncCommonRateControlMode_CBR),
            vbr ? "RateControlMode=VBR" : "RateControlMode=CBR");
        setUI4(CODECAPI_AVEncCommonMeanBitRate, (ULONG)cfg.bitrateBps, "MeanBitRate");
        if (cfg.lowLatency) {
            setBool(CODECAPI_AVEncCommonLowLatency, true, "CommonLowLatency");
            setBool(CODECAPI_AVLowLatencyMode, true, "LowLatencyMode");
        }
        setUI4(CODECAPI_AVEncMPVGOPSize, 0x7fffffff, "GOPSize");
        const deskhub::media::RatePlan plan =
            deskhub::media::PlanRateControl(cfg.bitrateBps, cfg.fps, cfg.lowLatency);
        setUI4(CODECAPI_AVEncCommonBufferSize, (ULONG)plan.vbvBits, "BufferSize(VBV)");
        setUI4(CODECAPI_AVEncCommonBufferInLevel, (ULONG)plan.vbvInitialBits, "BufferInLevel");
        rcLogged = true;
        return true;
    }

    bool SetBitrate(uint32_t bitrateBps) {
        if (!bitrateBps) return false;
        if (bitrateBps == cfg.bitrateBps) return true;
        cfg.bitrateBps = bitrateBps;
        if (codecApi && codecApi->IsSupported(&CODECAPI_AVEncCommonMeanBitRate) == S_OK) {
            VARIANT v{};
            v.vt = VT_UI4;
            v.ulVal = (ULONG)bitrateBps;
            if (SUCCEEDED(codecApi->SetValue(&CODECAPI_AVEncCommonMeanBitRate, &v))) return true;
        }
        if (!mft) return true;
        return ReinitTransform();
    }

    bool SetFps(uint32_t fps) {
        if (!fps || fps == cfg.fps) return fps == cfg.fps;
        cfg.fps = fps;
        if (!mft) return true;
        return ReinitTransform();
    }

    bool RequestKeyFrame() {
        ForgetLongTermReferences();
        if (SetCodecUI4(CODECAPI_AVEncVideoForceKeyFrame, 1)) return true;
        return ReinitTransform();
    }

    bool SetupColorConvert() {
        const uint32_t inW = cfg.srcWidth ? cfg.srcWidth : cfg.width;
        const uint32_t inH = cfg.srcHeight ? cfg.srcHeight : cfg.height;

        D3D11_TEXTURE2D_DESC td{};
        td.Width = cfg.width;
        td.Height = cfg.height;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_NV12;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        MF_CHECK(device->CreateTexture2D(&td, nullptr, &nv12Tex), "CreateTexture2D(NV12)");

        D3D11VideoProcessor::Setup s;
        s.srcWidth = inW;
        s.srcHeight = inH;
        s.dstWidth = cfg.width;
        s.dstHeight = cfg.height;
        s.srcRect = RECT{0, 0, (LONG)cfg.width, (LONG)cfg.height};
        s.dstRect = RECT{0, 0, (LONG)cfg.width, (LONG)cfg.height};
        s.dstYCbCr = true;
        s.requiredOutputFormat = DXGI_FORMAT_NV12;
        return colorConvert.Configure(device.Get(), nv12Tex.Get(), s, "MfEncoder");
    }

    bool ConvertToNv12(ID3D11Texture2D* bgra) {
        return colorConvert.Blt(bgra);
    }

    void CacheSpsPps() {
        ComPtr<IMFMediaType> curOut;
        if (FAILED(mft->GetOutputCurrentType(0, &curOut))) return;
        UINT32 size = 0;
        if (FAILED(curOut->GetBlobSize(MF_MT_MPEG_SEQUENCE_HEADER, &size)) || size == 0) return;
        spsPps.resize(size);
        if (FAILED(curOut->GetBlob(MF_MT_MPEG_SEQUENCE_HEADER, spsPps.data(), size, nullptr))) {
            spsPps.clear();
            return;
        }
        std::vector<uint8_t> zeroReorder =
            deskhub::media::AnnexBStreamWithZeroReorder(spsPps);
        if (zeroReorder.empty()) return;
        spsPps = std::move(zeroReorder);
        LOGI("[MfEncoder] SPS did not signal a reorder limit - added max_num_reorder_frames=0.");
    }

    bool EmitSample(IMFSample* sample) {
        ComPtr<IMFMediaBuffer> buffer;
        MF_CHECK(sample->ConvertToContiguousBuffer(&buffer), "ConvertToContiguousBuffer");
        BYTE* data = nullptr;
        DWORD len = 0;
        MF_CHECK(buffer->Lock(&data, nullptr, &len), "Lock(out)");

        const bool keyframe =
            deskhub::media::ContainsIdr(std::span<const uint8_t>(data, len));
        if (keyframe && spsPps.empty()) CacheSpsPps();

        LONGLONG timeHns = 0;
        sample->GetSampleTime(&timeHns);
        const uint64_t tsUs = firstTsUs + (uint64_t)(timeHns / 10);
        const bool prependHeader = keyframe && !spsPps.empty();

        if (out) {
            if (prependHeader) std::fwrite(spsPps.data(), 1, spsPps.size(), out);
            std::fwrite(data, 1, len, out);
        }
        if (cfg.onPacket && len > 0) {
            if (prependHeader) {
                std::vector<uint8_t> withHeader;
                withHeader.reserve(spsPps.size() + len);
                withHeader.insert(withHeader.end(), spsPps.begin(), spsPps.end());
                withHeader.insert(withHeader.end(), data, data + len);
                cfg.onPacket(withHeader.data(), withHeader.size(), tsUs, keyframe);
            } else {
                cfg.onPacket(data, len, tsUs, keyframe);
            }
        }
        totalBytes += len;
        ++frameCount;
        if (frameCount <= 5 || frameCount % 60 == 0) {
            LOGI("[MfEncoder] frame %llu: %lu byte%s", (unsigned long long)frameCount,
                len, keyframe ? " (IDR)" : "");
        }
        buffer->Unlock();
        return true;
    }

    bool RenegotiateOutputType() {
        for (DWORD i = 0;; ++i) {
            ComPtr<IMFMediaType> t;
            HRESULT hr = mft->GetOutputAvailableType(0, i, &t);
            if (hr == MF_E_NO_MORE_TYPES) break;
            if (FAILED(hr)) {
                LOGE("[MfEncoder] GetOutputAvailableType failed: 0x%08lX",
                    (unsigned long)hr);
                return false;
            }
            GUID sub{};
            t->GetGUID(MF_MT_SUBTYPE, &sub);
            if (sub != MFVideoFormat_H264) continue;
            if (SUCCEEDED(mft->SetOutputType(0, t.Get(), 0))) {
                MFT_OUTPUT_STREAM_INFO si{};
                if (SUCCEEDED(mft->GetOutputStreamInfo(0, &si))) {
                    outputProvidesSamples = (si.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES |
                                                              MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;
                }
                return true;
            }
        }
        LOGW("[MfEncoder] Could not find a suitable output type after STREAM_CHANGE.");
        return false;
    }

    int PullOneOutput() {
        const int maxAttempts = isAsync ? 1 : 2;
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            MFT_OUTPUT_DATA_BUFFER ob{};
            ob.dwStreamID = 0;
            ComPtr<IMFSample> ownedSample;
            if (!outputProvidesSamples) {
                MFT_OUTPUT_STREAM_INFO si{};
                MF_CHECKI(mft->GetOutputStreamInfo(0, &si), "GetOutputStreamInfo");
                ComPtr<IMFMediaBuffer> buf;
                MF_CHECKI(MFCreateMemoryBuffer(si.cbSize ? si.cbSize : (1u << 20), &buf),
                    "MFCreateMemoryBuffer(out)");
                MF_CHECKI(MFCreateSample(&ownedSample), "MFCreateSample(out)");
                MF_CHECKI(ownedSample->AddBuffer(buf.Get()), "AddBuffer(out)");
                ob.pSample = ownedSample.Get();
            }
            DWORD status = 0;
            HRESULT hr = mft->ProcessOutput(0, 1, &ob, &status);
            if (ob.pEvents) {
                ob.pEvents->Release();
                ob.pEvents = nullptr;
            }

            if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) return 0;
            if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
                if (ob.pSample && outputProvidesSamples) ob.pSample->Release();
                if (!RenegotiateOutputType()) return -1;
                continue;
            }
            if (FAILED(hr)) {
                if (ob.pSample && outputProvidesSamples) ob.pSample->Release();
                LOGE("[MfEncoder] ProcessOutput failed: 0x%08lX", (unsigned long)hr);
                return -1;
            }

            ComPtr<IMFSample> sample;
            if (outputProvidesSamples)
                sample.Attach(ob.pSample);
            else
                sample = ownedSample;
            if (!sample) return 0;
            return EmitSample(sample.Get()) ? 1 : -1;
        }
        return 0;
    }

    bool DrainOutputsSync() {
        for (;;) {
            int r = PullOneOutput();
            if (r < 0) return false;
            if (r == 0) return true;
        }
    }

    bool WaitForNeedInputAsync() {
        if (needInputCredit > 0) {
            --needInputCredit;
            return true;
        }
        for (int waitedMs = 0;;) {
            ComPtr<IMFMediaEvent> ev;
            const HRESULT hr = events->GetEvent(MF_EVENT_FLAG_NO_WAIT, &ev);
            if (hr == MF_E_NO_EVENTS_AVAILABLE) {
                if (waitedMs++ >= 1000) {
                    LOGW("[MfEncoder] Timed out waiting for encoder NeedInput.");
                    return false;
                }
                Sleep(1);
                continue;
            }
            MF_CHECK(hr, "GetEvent");
            MediaEventType met = MEUnknown;
            MF_CHECK(ev->GetType(&met), "GetType");
            if (met == METransformNeedInput) return true;
            if (met == METransformHaveOutput) {
                if (PullOneOutput() < 0) return false;
                continue;
            }
        }
    }

    bool PumpAsyncEvents(bool waitForOutput) {
        int sleepBudgetMs = 30;
        for (;;) {
            ComPtr<IMFMediaEvent> ev;
            const HRESULT hr = events->GetEvent(MF_EVENT_FLAG_NO_WAIT, &ev);
            if (hr == MF_E_NO_EVENTS_AVAILABLE) {
                if (!waitForOutput || sleepBudgetMs-- <= 0) return true;
                Sleep(1);
                continue;
            }
            if (FAILED(hr)) return true;
            MediaEventType met = MEUnknown;
            ev->GetType(&met);
            if (met == METransformNeedInput) {
                ++needInputCredit;
            } else if (met == METransformHaveOutput) {
                const int r = PullOneOutput();
                if (r < 0) return false;
                if (r > 0) waitForOutput = false;
            }
        }
    }

    bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) {
        if (!streaming) return false;
        if (!ConvertToNv12(frame)) return false;
        if (forceKeyframe && !RequestKeyFrame()) return false;

        if (isAsync && !WaitForNeedInputAsync()) return false;

        ComPtr<IMFMediaBuffer> buffer;
        MF_CHECK(MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), nv12Tex.Get(), 0, FALSE,
                     &buffer),
            "MFCreateDXGISurfaceBuffer");
        ComPtr<IMF2DBuffer2> buf2d;
        if (SUCCEEDED(buffer.As(&buf2d))) {
            DWORD len = 0;
            if (SUCCEEDED(buf2d->GetContiguousLength(&len))) buffer->SetCurrentLength(len);
        }

        ComPtr<IMFSample> sample;
        MF_CHECK(MFCreateSample(&sample), "MFCreateSample");
        MF_CHECK(sample->AddBuffer(buffer.Get()), "AddBuffer");

        if (!haveFirstTs) {
            firstTsUs = timestampUs;
            haveFirstTs = true;
        }
        const LONGLONG timeHns = static_cast<LONGLONG>((timestampUs - firstTsUs) * 10ull);
        const LONGLONG durHns = static_cast<LONGLONG>(10'000'000ull / (cfg.fps ? cfg.fps : 60));
        sample->SetSampleTime(timeHns);
        sample->SetSampleDuration(durHns);

        HRESULT hr = mft->ProcessInput(0, sample.Get(), 0);
        if (!isAsync && hr == MF_E_NOTACCEPTING) {
            if (!DrainOutputsSync()) return false;
            hr = mft->ProcessInput(0, sample.Get(), 0);
        }
        MF_CHECK(hr, "ProcessInput");

        if (isAsync) {
            const ULONGLONG nowMs = GetTickCount64();
            const bool sparse = lastEncodeTickMs && nowMs - lastEncodeTickMs > 100;
            lastEncodeTickMs = nowMs;
            return PumpAsyncEvents(sparse);
        }
        return DrainOutputsSync();
    }

    void Finish() {
        if (!streaming) return;
        mft->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
        for (int i = 0; i < 256; ++i) {
            if (isAsync) {
                ComPtr<IMFMediaEvent> ev;
                if (FAILED(events->GetEvent(0, &ev))) break;
                MediaEventType met = MEUnknown;
                ev->GetType(&met);
                if (met == METransformHaveOutput) {
                    if (PullOneOutput() < 0) break;
                    continue;
                }
                if (met == METransformDrainComplete) break;
            } else {
                int r = PullOneOutput();
                if (r < 0) break;
                if (r == 0) break;
            }
        }
        mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
        mft->ProcessMessage(MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
        streaming = false;
        if (out) std::fflush(out);
        LOGI("[MfEncoder] Encoded %llu frame, %.2f MB.",
            (unsigned long long)frameCount, totalBytes / 1e6);
    }
};

MfEncoder::MfEncoder() = default;
MfEncoder::~MfEncoder() = default;

bool MfEncoder::Init(ID3D11Device* device, const EncoderConfig& cfg) {
    impl_ = std::make_unique<Impl>();
    if (!impl_->Init(device, cfg)) {
        impl_.reset();
        return false;
    }
    return true;
}

bool MfEncoder::Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) {
    return impl_ && impl_->Encode(frame, timestampUs, forceKeyframe);
}

bool MfEncoder::SetBitrate(uint32_t bitrateBps) {
    return impl_ && impl_->SetBitrate(bitrateBps);
}

bool MfEncoder::SetFps(uint32_t fps) {
    return impl_ && impl_->SetFps(fps);
}

deskhub::media::EncoderRecoveryCaps MfEncoder::RecoveryCaps() const {
    return impl_ ? impl_->recovery : deskhub::media::EncoderRecoveryCaps{};
}
bool MfEncoder::MarkLongTermReference(uint32_t frameId) {
    return impl_ && impl_->MarkLongTermReference(frameId);
}
bool MfEncoder::InvalidateReference(uint32_t firstInvalidFrameId) {
    return impl_ && impl_->InvalidateReference(firstInvalidFrameId);
}
bool MfEncoder::BeginIntraRefresh(uint32_t frames) {
    return impl_ && impl_->BeginIntraRefresh(frames);
}

void MfEncoder::Finish() {
    if (impl_) impl_->Finish();
}
