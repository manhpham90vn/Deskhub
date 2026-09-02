#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "encode/NvencEncoder.h"

#include <windows.h>
#include <d3d11.h>
#include <nvEncodeAPI.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "deskhub/media/H264Sps.h"
#include "deskhub/media/RatePlan.h"
#include "deskhubp/diag/Log.h"

using PFN_CreateInstance = NVENCSTATUS(NVENCAPI*)(NV_ENCODE_API_FUNCTION_LIST*);
using PFN_MaxVersion = NVENCSTATUS(NVENCAPI*)(uint32_t*);

struct NvencEncoder::Impl {
    HMODULE dll = nullptr;
    NV_ENCODE_API_FUNCTION_LIST nv{};
    void* enc = nullptr;
    NV_ENC_OUTPUT_PTR bitstream = nullptr;
    FILE* out = nullptr;
    EncoderConfig cfg{};
    deskhub::media::EncoderRecoveryCaps recovery{};
    uint32_t width = 0, height = 0;
    NV_ENC_CONFIG encCfg{};
    NV_ENC_INITIALIZE_PARAMS initParams{};
    bool loggedZeroReorder = false;
    uint64_t frameCount = 0;
    uint64_t totalBytes = 0;

    std::map<ID3D11Texture2D*, NV_ENC_REGISTERED_PTR> registered;

    ~Impl() {
        Cleanup();
    }

    bool Fail(const char* where, NVENCSTATUS s) {
        const char* msg = (nv.nvEncGetLastErrorString && enc) ? nv.nvEncGetLastErrorString(enc) : "";
        LOGE("[NVENC] %s failed: status=%d %s", where, (int)s, msg ? msg : "");
        return false;
    }

    bool Init(ID3D11Device* device, const EncoderConfig& c) {
        if ((c.srcWidth && c.srcWidth != c.width) || (c.srcHeight && c.srcHeight != c.height)) {
            LOGE(
                "[NVENC] Cannot crop %ux%u input to %ux%u \xE2\x80\x94 leaving this to the "
                "Media Foundation backend.",
                c.srcWidth, c.srcHeight, c.width, c.height);
            return false;
        }

        cfg = c;
        width = c.width;
        height = c.height;

        dll = LoadLibraryW(L"nvEncodeAPI64.dll");
        if (!dll) {
            LOGE("[NVENC] Failed to load nvEncodeAPI64.dll (NVIDIA driver missing?).");
            return false;
        }

        auto getMax = (PFN_MaxVersion)GetProcAddress(dll, "NvEncodeAPIGetMaxSupportedVersion");
        if (getMax) {
            uint32_t driverMax = 0;
            if (getMax(&driverMax) == NV_ENC_SUCCESS) {
                uint32_t needed = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
                if (driverMax < needed) {
                    LOGW("[NVENC] Driver older than header (driver=%u.%u < required=%u.%u).",
                        driverMax >> 4, driverMax & 0xf,
                        NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION);
                    return false;
                }
            }
        }

        auto createInstance = (PFN_CreateInstance)GetProcAddress(dll, "NvEncodeAPICreateInstance");
        if (!createInstance) {
            LOGE("[NVENC] Missing NvEncodeAPICreateInstance.");
            return false;
        }

        nv = {};
        nv.version = NV_ENCODE_API_FUNCTION_LIST_VER;
        NVENCSTATUS s = createInstance(&nv);
        if (s != NV_ENC_SUCCESS) {
            LOGE("[NVENC] CreateInstance status=%d", (int)s);
            return false;
        }

        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sp{};
        sp.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
        sp.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        sp.device = device;
        sp.apiVersion = NVENCAPI_VERSION;
        s = nv.nvEncOpenEncodeSessionEx(&sp, &enc);
        if (s != NV_ENC_SUCCESS) {
            enc = nullptr;
            return Fail("OpenEncodeSessionEx", s);
        }

        const GUID codecGuid = NV_ENC_CODEC_H264_GUID;
        const GUID presetGuid = NV_ENC_PRESET_P4_GUID;
        recovery = QueryRecoveryCaps(codecGuid);
        const NV_ENC_TUNING_INFO tuning = cfg.lowLatency ? NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY
                                                         : NV_ENC_TUNING_INFO_HIGH_QUALITY;

        NV_ENC_PRESET_CONFIG preset{};
        preset.version = NV_ENC_PRESET_CONFIG_VER;
        preset.presetCfg.version = NV_ENC_CONFIG_VER;
        s = nv.nvEncGetEncodePresetConfigEx(enc, codecGuid, presetGuid, tuning, &preset);
        if (s != NV_ENC_SUCCESS) return Fail("GetEncodePresetConfigEx", s);

        encCfg = preset.presetCfg;
        encCfg.gopLength = NVENC_INFINITE_GOPLENGTH;
        encCfg.frameIntervalP = 1;
        encCfg.rcParams.rateControlMode =
            cfg.rc == RateControl::VBR ? NV_ENC_PARAMS_RC_VBR : NV_ENC_PARAMS_RC_CBR;
        encCfg.rcParams.averageBitRate = cfg.bitrateBps;
        ApplyRatePlan(cfg.fps);
        encCfg.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
        encCfg.encodeCodecConfig.h264Config.repeatSPSPPS = 1;

        initParams = {};
        NV_ENC_INITIALIZE_PARAMS& ip = initParams;
        ip.version = NV_ENC_INITIALIZE_PARAMS_VER;
        ip.encodeGUID = codecGuid;
        ip.presetGUID = presetGuid;
        ip.tuningInfo = tuning;
        ip.encodeWidth = width;
        ip.encodeHeight = height;
        ip.darWidth = width;
        ip.darHeight = height;
        ip.frameRateNum = cfg.fps ? cfg.fps : 60;
        ip.frameRateDen = 1;
        ip.enablePTD = 1;
        ip.enableEncodeAsync = 0;
        ip.encodeConfig = &encCfg;
        s = nv.nvEncInitializeEncoder(enc, &ip);
        if (s != NV_ENC_SUCCESS) return Fail("InitializeEncoder", s);

        NV_ENC_CREATE_BITSTREAM_BUFFER cb{};
        cb.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
        s = nv.nvEncCreateBitstreamBuffer(enc, &cb);
        if (s != NV_ENC_SUCCESS) return Fail("CreateBitstreamBuffer", s);
        bitstream = cb.bitstreamBuffer;

        if (!OpenEncoderOutput(cfg, "NVENC", out)) return false;

        LOGI("[NVENC] Initialized: %ux%u @%ufps, %.1f Mbps, H264, %s -> %s", width, height,
            cfg.fps, cfg.bitrateBps / 1e6,
            cfg.lowLatency ? "ULTRA_LOW_LATENCY" : "HIGH_QUALITY",
            out ? "file" : "callback");
        return true;
    }

    int Cap(const GUID& codecGuid, NV_ENC_CAPS which) {
        NV_ENC_CAPS_PARAM param{};
        param.version = NV_ENC_CAPS_PARAM_VER;
        param.capsToQuery = which;
        int value = 0;
        if (nv.nvEncGetEncodeCaps(enc, codecGuid, &param, &value) != NV_ENC_SUCCESS) return 0;
        return value;
    }

    deskhub::media::EncoderRecoveryCaps QueryRecoveryCaps(const GUID& codecGuid) {
        const int ltrFrames = Cap(codecGuid, NV_ENC_CAPS_NUM_MAX_LTR_FRAMES);
        const int invalidation = Cap(codecGuid, NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION);
        const int refresh = Cap(codecGuid, NV_ENC_CAPS_SUPPORT_INTRA_REFRESH);
        LOGI("[NVENC] recovery caps: max_ltr_frames=%d ref_pic_invalidation=%d intra_refresh=%d",
            ltrFrames, invalidation, refresh);
        return {ltrFrames > 0 && invalidation != 0, refresh != 0};
    }

    void ApplyRatePlan(uint32_t fps) {
        const deskhub::media::RatePlan plan =
            deskhub::media::PlanRateControl(cfg.bitrateBps, fps, cfg.lowLatency);
        encCfg.rcParams.vbvBufferSize = plan.vbvBits;
        encCfg.rcParams.vbvInitialDelay = plan.vbvInitialBits;
    }

    bool SetBitrate(uint32_t bitrateBps) {
        if (!enc || !bitrateBps) return false;
        cfg.bitrateBps = bitrateBps;
        encCfg.rcParams.averageBitRate = bitrateBps;
        ApplyRatePlan(cfg.fps);

        NV_ENC_RECONFIGURE_PARAMS rp{};
        rp.version = NV_ENC_RECONFIGURE_PARAMS_VER;
        rp.reInitEncodeParams = initParams;
        NVENCSTATUS s = nv.nvEncReconfigureEncoder(enc, &rp);
        if (s != NV_ENC_SUCCESS) return Fail("ReconfigureEncoder", s);
        return true;
    }

    bool SetFps(uint32_t fps) {
        if (!enc || !fps) return false;
        if (fps == cfg.fps) return true;
        cfg.fps = fps;
        initParams.frameRateNum = fps;
        initParams.frameRateDen = 1;
        ApplyRatePlan(fps);

        NV_ENC_RECONFIGURE_PARAMS rp{};
        rp.version = NV_ENC_RECONFIGURE_PARAMS_VER;
        rp.reInitEncodeParams = initParams;
        rp.resetEncoder = 0;
        NVENCSTATUS s = nv.nvEncReconfigureEncoder(enc, &rp);
        if (s != NV_ENC_SUCCESS) return Fail("ReconfigureEncoder(fps)", s);
        return true;
    }

    NV_ENC_REGISTERED_PTR RegisterTex(ID3D11Texture2D* tex) {
        auto it = registered.find(tex);
        if (it != registered.end()) return it->second;

        NV_ENC_REGISTER_RESOURCE rr{};
        rr.version = NV_ENC_REGISTER_RESOURCE_VER;
        rr.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
        rr.width = width;
        rr.height = height;
        rr.pitch = 0;
        rr.resourceToRegister = tex;
        rr.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
        NVENCSTATUS s = nv.nvEncRegisterResource(enc, &rr);
        if (s != NV_ENC_SUCCESS) {
            Fail("RegisterResource", s);
            return nullptr;
        }
        registered[tex] = rr.registeredResource;
        return rr.registeredResource;
    }

    bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) {
        if (!enc) return false;

        NV_ENC_REGISTERED_PTR reg = RegisterTex(frame);
        if (!reg) return false;

        NV_ENC_MAP_INPUT_RESOURCE mp{};
        mp.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
        mp.registeredResource = reg;
        NVENCSTATUS s = nv.nvEncMapInputResource(enc, &mp);
        if (s != NV_ENC_SUCCESS) return Fail("MapInputResource", s);

        NV_ENC_PIC_PARAMS pp{};
        pp.version = NV_ENC_PIC_PARAMS_VER;
        pp.inputWidth = width;
        pp.inputHeight = height;
        pp.inputPitch = 0;
        pp.inputBuffer = mp.mappedResource;
        pp.bufferFmt = mp.mappedBufferFmt;
        pp.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        pp.outputBitstream = bitstream;
        pp.inputTimeStamp = timestampUs;
        if (forceKeyframe) pp.encodePicFlags |= NV_ENC_PIC_FLAG_FORCEIDR | NV_ENC_PIC_FLAG_OUTPUT_SPSPPS;

        s = nv.nvEncEncodePicture(enc, &pp);
        bool ok = true;
        if (s == NV_ENC_SUCCESS) {
            ok = WriteOutput();
        } else if (s != NV_ENC_ERR_NEED_MORE_INPUT) {
            ok = Fail("EncodePicture", s);
        }

        nv.nvEncUnmapInputResource(enc, mp.mappedResource);
        return ok;
    }

    bool WriteOutput() {
        NV_ENC_LOCK_BITSTREAM lb{};
        lb.version = NV_ENC_LOCK_BITSTREAM_VER;
        lb.outputBitstream = bitstream;
        NVENCSTATUS s = nv.nvEncLockBitstream(enc, &lb);
        if (s != NV_ENC_SUCCESS) return Fail("LockBitstream", s);

        const bool keyframe = (lb.pictureType == NV_ENC_PIC_TYPE_IDR);
        const uint8_t* data = (const uint8_t*)lb.bitstreamBufferPtr;
        size_t len = lb.bitstreamSizeInBytes;

        std::vector<uint8_t> zeroReorder;
        if (keyframe && len) {
            zeroReorder =
                deskhub::media::AnnexBStreamWithZeroReorder(std::span<const uint8_t>(data, len));
            if (!zeroReorder.empty()) {
                data = zeroReorder.data();
                len = zeroReorder.size();
                if (!loggedZeroReorder) {
                    loggedZeroReorder = true;
                    LOGI(
                        "[NVENC] SPS did not signal a reorder limit -"
                        " added max_num_reorder_frames=0.");
                }
            }
        }

        if (out) std::fwrite(data, 1, len, out);
        if (cfg.onPacket && len > 0) cfg.onPacket(data, len, lb.outputTimeStamp, keyframe);
        totalBytes += len;
        ++frameCount;
        if (frameCount <= 5 || frameCount % 60 == 0) {
            LOGI("[NVENC] frame %llu: %u byte%s", (unsigned long long)frameCount,
                lb.bitstreamSizeInBytes, keyframe ? " (IDR)" : "");
        }
        nv.nvEncUnlockBitstream(enc, bitstream);
        return true;
    }

    void Finish() {
        if (!enc) return;
        NV_ENC_PIC_PARAMS eos{};
        eos.version = NV_ENC_PIC_PARAMS_VER;
        eos.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
        if (nv.nvEncEncodePicture(enc, &eos) == NV_ENC_SUCCESS) WriteOutput();
        if (out) std::fflush(out);
        LOGI("[NVENC] Encoded %llu frame, %.2f MB.",
            (unsigned long long)frameCount, totalBytes / 1e6);
    }

    void Cleanup() {
        if (enc) {
            for (auto& kv : registered) nv.nvEncUnregisterResource(enc, kv.second);
            registered.clear();
            if (bitstream) {
                nv.nvEncDestroyBitstreamBuffer(enc, bitstream);
                bitstream = nullptr;
            }
            nv.nvEncDestroyEncoder(enc);
            enc = nullptr;
        }
        if (out) {
            std::fclose(out);
            out = nullptr;
        }
        if (dll) {
            FreeLibrary(dll);
            dll = nullptr;
        }
    }
};

NvencEncoder::NvencEncoder() = default;
NvencEncoder::~NvencEncoder() = default;

bool NvencEncoder::Init(ID3D11Device* device, const EncoderConfig& cfg) {
    impl_ = std::make_unique<Impl>();
    if (!impl_->Init(device, cfg)) {
        impl_.reset();
        return false;
    }
    return true;
}
bool NvencEncoder::Encode(ID3D11Texture2D* frame, uint64_t ts, bool forceKeyframe) {
    return impl_ && impl_->Encode(frame, ts, forceKeyframe);
}
bool NvencEncoder::SetBitrate(uint32_t bitrateBps) {
    return impl_ && impl_->SetBitrate(bitrateBps);
}

bool NvencEncoder::SetFps(uint32_t fps) {
    return impl_ && impl_->SetFps(fps);
}
deskhub::media::EncoderRecoveryCaps NvencEncoder::RecoveryCaps() const {
    return impl_ ? impl_->recovery : deskhub::media::EncoderRecoveryCaps{};
}
void NvencEncoder::Finish() {
    if (impl_) impl_->Finish();
}
