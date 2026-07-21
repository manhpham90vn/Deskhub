// =============================================================================
// NvencEncoder.cpp — cài đặt backend NVENC.
//
// VÒNG ĐỜI MỘT FRAME QUA NVENC — bốn bước, và mỗi bước có một cái bẫy riêng
//   1. REGISTER  — báo cho NVENC biết một texture D3D11 (làm MỘT LẦN cho mỗi
//                  texture, kết quả nhớ trong `registered`).
//   2. MAP       — khoá texture đã đăng ký để dùng cho lần nén này.
//   3. ENCODE    — nvEncEncodePicture.
//   4. UNMAP     — nhả ra. BẮT BUỘC, kể cả khi bước 3 thất bại; thiếu là rò tài
//                  nguyên và vài frame sau sẽ hết chỗ map.
//
// VÌ SAO PHẢI NHỚ ĐĂNG KÝ (map `registered`)
//   nvEncRegisterResource khá đắt. WGC chỉ luân phiên vài texture cố định (frame
//   pool sâu 2), nên đăng ký lại mỗi frame là lãng phí thuần tuý. Khoá theo con trỏ
//   texture; các mục được huỷ đăng ký một lượt trong Cleanup().
//
// TẠI SAO encCfg VÀ initParams LÀ THÀNH VIÊN, KHÔNG PHẢI BIẾN CỤC BỘ CỦA Init()
//   nvEncReconfigureEncoder (đổi bitrate) đòi NGUYÊN bộ tham số khởi tạo chứ không
//   nhận riêng trường cần đổi, nên ta phải giữ chúng sống suốt phiên. Quan trọng
//   hơn: initParams.encodeConfig là CON TRỎ trỏ vào encCfg — nếu encCfg là biến cục
//   bộ của Init thì con trỏ đó thành treo ngay khi Init trả về, và lần đổi bitrate
//   đầu tiên sẽ đọc phải bộ nhớ rác.
//
// GOP VÔ HẠN — quyết định xuyên suốt cả dự án
//   gopLength = NVENC_INFINITE_GOPLENGTH: encoder KHÔNG tự phát IDR định kỳ. IDR chỉ
//   ra khi có người xin (forceKeyframe). Lý do: IDR nặng gấp nhiều lần P-frame, phát
//   đều đặn là đốt băng thông vô ích khi đường truyền đang tốt. Đổi lại, phía nhận
//   phải chủ động xin IDR mỗi khi mất frame — xem Reassembler.h.
//
// KHÔNG B-FRAME (frameIntervalP = 1)
//   B-frame tham chiếu cả frame TƯƠNG LAI, nên encoder phải giữ lại vài frame trước
//   khi xuất được cái đầu tiên. Với streaming tương tác thì độ trễ đó không đáng đổi
//   lấy chút bitrate tiết kiệm được.
//
// LIÊN QUAN: encode/NvencEncoder.h (vì sao NVENC đứng đầu chuỗi + nạp DLL động)
// =============================================================================
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
#include <string>

// Nạp DLL động: không link .lib, chỉ cần DLL đi kèm driver.
using PFN_CreateInstance = NVENCSTATUS(NVENCAPI*)(NV_ENCODE_API_FUNCTION_LIST*);
using PFN_MaxVersion = NVENCSTATUS(NVENCAPI*)(uint32_t*);

struct NvencEncoder::Impl {
    HMODULE                    dll = nullptr;
    NV_ENCODE_API_FUNCTION_LIST nv{};
    void*                      enc = nullptr;         // encode session handle
    NV_ENC_OUTPUT_PTR          bitstream = nullptr;   // 1 buffer (đồng bộ, không B-frame)
    FILE*                      out = nullptr;
    EncoderConfig              cfg{};
    uint32_t                   width = 0, height = 0;
    // Giữ lại để nvEncReconfigureEncoder (đổi bitrate) — API đòi cả bộ tham số khởi
    // tạo chứ không nhận riêng trường cần đổi. initParams.encodeConfig phải trỏ vào
    // encCfg (thành viên), không phải biến cục bộ của Init.
    NV_ENC_CONFIG              encCfg{};
    NV_ENC_INITIALIZE_PARAMS   initParams{};
    uint64_t                   frameCount = 0;
    uint64_t                   totalBytes = 0;

    // Cache đăng ký theo con trỏ texture: WGC dùng lại vài texture (pool depth 2).
    std::map<ID3D11Texture2D*, NV_ENC_REGISTERED_PTR> registered;

    ~Impl() { Cleanup(); }

    bool Fail(const char* where, NVENCSTATUS s) {
        const char* msg = (nv.nvEncGetLastErrorString && enc) ? nv.nvEncGetLastErrorString(enc) : "";
        std::printf("[NVENC] %s failed: status=%d %s\n", where, (int)s, msg ? msg : "");
        return false;
    }

    bool Init(ID3D11Device* device, const EncoderConfig& c) {
        cfg = c;
        width = c.width;
        height = c.height;

        dll = LoadLibraryW(L"nvEncodeAPI64.dll");
        if (!dll) { std::printf("[NVENC] Failed to load nvEncodeAPI64.dll (NVIDIA driver missing?).\n"); return false; }

        // Driver cũ hơn header SDK ta dịch cùng: các struct đã đổi bố cục nên gọi
        // vào sẽ hỏng theo cách khó đoán. Phát hiện sớm và trả false để factory rớt
        // sang Media Foundation — an toàn hơn nhiều so với để nó chạy rồi sập.
        auto getMax = (PFN_MaxVersion)GetProcAddress(dll, "NvEncodeAPIGetMaxSupportedVersion");
        if (getMax) {
            uint32_t driverMax = 0;
            if (getMax(&driverMax) == NV_ENC_SUCCESS) {
                uint32_t needed = (NVENCAPI_MAJOR_VERSION << 4) | NVENCAPI_MINOR_VERSION;
                if (driverMax < needed) {
                    std::printf("[NVENC] Driver older than header (driver=%u.%u < required=%u.%u).\n",
                        driverMax >> 4, driverMax & 0xf,
                        NVENCAPI_MAJOR_VERSION, NVENCAPI_MINOR_VERSION);
                    return false;
                }
            }
        }

        auto createInstance = (PFN_CreateInstance)GetProcAddress(dll, "NvEncodeAPICreateInstance");
        if (!createInstance) { std::printf("[NVENC] Missing NvEncodeAPICreateInstance.\n"); return false; }

        nv = {};
        nv.version = NV_ENCODE_API_FUNCTION_LIST_VER;
        NVENCSTATUS s = createInstance(&nv);
        if (s != NV_ENC_SUCCESS) { std::printf("[NVENC] CreateInstance status=%d\n", (int)s); return false; }

        // Mở session trên chính D3D11 device dùng chung với capture.
        NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS sp{};
        sp.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
        sp.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
        sp.device = device;
        sp.apiVersion = NVENCAPI_VERSION;
        s = nv.nvEncOpenEncodeSessionEx(&sp, &enc);
        if (s != NV_ENC_SUCCESS) { enc = nullptr; return Fail("OpenEncodeSessionEx", s); }

        const GUID codecGuid = (cfg.codec == Codec::HEVC) ? NV_ENC_CODEC_HEVC_GUID
                                                          : NV_ENC_CODEC_H264_GUID;
        const GUID presetGuid = NV_ENC_PRESET_P4_GUID;
        const NV_ENC_TUNING_INFO tuning = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;

        // Lấy cấu hình preset làm nền rồi mới tinh chỉnh, thay vì điền tay từ số 0:
        // NV_ENC_CONFIG có hàng chục trường, phần lớn ta không có ý kiến gì. Xin bộ
        // mặc định của preset rồi sửa đúng vài trường mình quan tâm là cách vừa gọn
        // vừa bền qua các đời SDK.
        NV_ENC_PRESET_CONFIG preset{};
        preset.version = NV_ENC_PRESET_CONFIG_VER;
        preset.presetCfg.version = NV_ENC_CONFIG_VER;
        s = nv.nvEncGetEncodePresetConfigEx(enc, codecGuid, presetGuid, tuning, &preset);
        if (s != NV_ENC_SUCCESS) return Fail("GetEncodePresetConfigEx", s);

        encCfg = preset.presetCfg;
        encCfg.gopLength = NVENC_INFINITE_GOPLENGTH;   // IDR theo yêu cầu, không định kỳ
        encCfg.frameIntervalP = 1;                     // không B-frame (độ trễ thấp)
        encCfg.rcParams.rateControlMode = NV_ENC_PARAMS_RC_CBR;
        encCfg.rcParams.averageBitRate = cfg.bitrateBps;
        // VBV buffer ~ đúng một frame. Đây là "ngân sách dồn" mà encoder được phép
        // vượt tạm: đặt nhỏ thì kích thước từng frame đều đặn, đổi lại chất lượng
        // dao động ở cảnh động. Với streaming thì đều đặn quan trọng hơn — một frame
        // phình to đột ngột chính là thứ tạo ra chùm gói mà Pacer đang phải chống.
        encCfg.rcParams.vbvBufferSize = cfg.bitrateBps / (cfg.fps ? cfg.fps : 60);
        encCfg.rcParams.vbvInitialDelay = encCfg.rcParams.vbvBufferSize;
        if (cfg.codec == Codec::HEVC) {
            encCfg.encodeCodecConfig.hevcConfig.idrPeriod = NVENC_INFINITE_GOPLENGTH;
            encCfg.encodeCodecConfig.hevcConfig.repeatSPSPPS = 1;
        } else {
            encCfg.encodeCodecConfig.h264Config.idrPeriod = NVENC_INFINITE_GOPLENGTH;
            // repeatSPSPPS: nhét SPS/PPS vào TRƯỚC MỖI IDR thay vì chỉ một lần đầu
            // stream. Bắt buộc với UDP: client vào giữa chừng, hoặc vừa mất gói rồi
            // xin IDR, sẽ không có tham số giải mã nếu chúng chỉ được gửi lúc đầu.
            encCfg.encodeCodecConfig.h264Config.repeatSPSPPS = 1;
        }

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
        ip.enablePTD = 1;                 // NVENC tự quyết định loại picture
        ip.enableEncodeAsync = 0;         // đồng bộ cho đơn giản
        ip.encodeConfig = &encCfg;
        s = nv.nvEncInitializeEncoder(enc, &ip);
        if (s != NV_ENC_SUCCESS) return Fail("InitializeEncoder", s);

        // Buffer bitstream đầu ra.
        NV_ENC_CREATE_BITSTREAM_BUFFER cb{};
        cb.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
        s = nv.nvEncCreateBitstreamBuffer(enc, &cb);
        if (s != NV_ENC_SUCCESS) return Fail("CreateBitstreamBuffer", s);
        bitstream = cb.bitstreamBuffer;

        // NVENC xuất Annex-B thô. File .h264 là tùy chọn (rỗng = chỉ đi qua onPacket).
        std::wstring path = cfg.outputPath;
        if (!path.empty()) {
            size_t dot = path.find_last_of(L'.');
            if (dot != std::wstring::npos && path.substr(dot) == L".mp4") path = path.substr(0, dot) + L".h264";
            out = _wfopen(path.c_str(), L"wb");
            if (!out) { std::printf("[NVENC] Failed to open output file.\n"); return false; }
        } else if (!cfg.onPacket) {
            std::printf("[NVENC] No outputPath or onPacket - no output destination.\n");
            return false;
        }

        std::printf("[NVENC] Initialized: %ux%u @%ufps, %.1f Mbps, %s, ULTRA_LOW_LATENCY -> %ls\n",
            width, height, cfg.fps, cfg.bitrateBps / 1e6,
            cfg.codec == Codec::HEVC ? "HEVC" : "H264",
            path.empty() ? L"callback" : path.c_str());
        return true;
    }

    // Đổi bitrate không dựng lại session: chuỗi inter-frame giữ nguyên, không cần IDR.
    bool SetBitrate(uint32_t bitrateBps) {
        if (!enc || !bitrateBps) return false;
        cfg.bitrateBps = bitrateBps;
        encCfg.rcParams.averageBitRate = bitrateBps;
        encCfg.rcParams.vbvBufferSize = bitrateBps / (cfg.fps ? cfg.fps : 60);
        encCfg.rcParams.vbvInitialDelay = encCfg.rcParams.vbvBufferSize;

        NV_ENC_RECONFIGURE_PARAMS rp{};
        rp.version = NV_ENC_RECONFIGURE_PARAMS_VER;
        rp.reInitEncodeParams = initParams;      // encodeConfig vẫn trỏ vào encCfg
        NVENCSTATUS s = nv.nvEncReconfigureEncoder(enc, &rp);
        if (s != NV_ENC_SUCCESS) return Fail("ReconfigureEncoder", s);
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
        // ARGB trong cách gọi tên của NVENC chính là B8G8R8A8 mà WGC giao ra. Khác
        // MfEncoder, NVENC nhận thẳng định dạng này và tự chuyển sang NV12 bên trong
        // — không cần video processor riêng.
        rr.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
        NVENCSTATUS s = nv.nvEncRegisterResource(enc, &rr);
        if (s != NV_ENC_SUCCESS) { Fail("RegisterResource", s); return nullptr; }
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
            // NEED_MORE_INPUT không phải lỗi: encoder đã nhận frame nhưng chưa xuất
            // gì. Cấu hình hiện tại (không B-frame) hiếm khi rơi vào đây, nhưng bỏ
            // qua nó cho đúng hợp đồng API.
            ok = Fail("EncodePicture", s);
        }

        // Unmap DÙ THÀNH CÔNG HAY KHÔNG — xem bước 4 ở đầu file. Đây là lý do hàm
        // không return sớm ở nhánh lỗi phía trên mà đi qua biến `ok`.
        nv.nvEncUnmapInputResource(enc, mp.mappedResource);
        return ok;
    }

    // Lấy dữ liệu đã nén ra khỏi buffer bitstream và giao cho hai đường ra.
    // Lock/Unlock phải đi thành cặp: giữ khoá lâu là chặn encoder ghi frame kế tiếp.
    bool WriteOutput() {
        NV_ENC_LOCK_BITSTREAM lb{};
        lb.version = NV_ENC_LOCK_BITSTREAM_VER;
        lb.outputBitstream = bitstream;
        NVENCSTATUS s = nv.nvEncLockBitstream(enc, &lb);
        if (s != NV_ENC_SUCCESS) return Fail("LockBitstream", s);

        const bool keyframe = (lb.pictureType == NV_ENC_PIC_TYPE_IDR);
        if (out) std::fwrite(lb.bitstreamBufferPtr, 1, lb.bitstreamSizeInBytes, out);
        if (cfg.onPacket && lb.bitstreamSizeInBytes > 0) {
            cfg.onPacket((const uint8_t*)lb.bitstreamBufferPtr, lb.bitstreamSizeInBytes,
                         lb.outputTimeStamp, keyframe);
        }
        totalBytes += lb.bitstreamSizeInBytes;
        ++frameCount;
        // In 5 frame đầu (để thấy stream đã khởi động đúng) rồi thưa dần mỗi giây —
        // in từng frame ở 60 fps sẽ làm console thành thứ không đọc được.
        if (frameCount <= 5 || frameCount % 60 == 0) {
            std::printf("[NVENC] frame %llu: %u byte%s\n", (unsigned long long)frameCount,
                lb.bitstreamSizeInBytes, keyframe ? " (IDR)" : "");
        }
        nv.nvEncUnlockBitstream(enc, bitstream);
        return true;
    }

    void Finish() {
        if (!enc) return;
        // Gửi EOS để flush nốt.
        NV_ENC_PIC_PARAMS eos{};
        eos.version = NV_ENC_PIC_PARAMS_VER;
        eos.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
        if (nv.nvEncEncodePicture(enc, &eos) == NV_ENC_SUCCESS) WriteOutput();
        if (out) std::fflush(out);
        std::printf("[NVENC] Encoded %llu frame, %.2f MB.\n",
            (unsigned long long)frameCount, totalBytes / 1e6);
    }

    // Dọn theo đúng thứ tự ngược với lúc tạo: tài nguyên đã đăng ký → buffer →
    // session → file → DLL. Huỷ session trước khi huỷ đăng ký sẽ làm các handle
    // registered thành vô nghĩa.
    void Cleanup() {
        if (enc) {
            for (auto& kv : registered) nv.nvEncUnregisterResource(enc, kv.second);
            registered.clear();
            if (bitstream) { nv.nvEncDestroyBitstreamBuffer(enc, bitstream); bitstream = nullptr; }
            nv.nvEncDestroyEncoder(enc);
            enc = nullptr;
        }
        if (out) { std::fclose(out); out = nullptr; }
        if (dll) { FreeLibrary(dll); dll = nullptr; }
    }
};

NvencEncoder::NvencEncoder() = default;
NvencEncoder::~NvencEncoder() = default;

bool NvencEncoder::Init(ID3D11Device* device, const EncoderConfig& cfg) {
    impl_ = std::make_unique<Impl>();
    if (!impl_->Init(device, cfg)) { impl_.reset(); return false; }
    return true;
}
bool NvencEncoder::Encode(ID3D11Texture2D* frame, uint64_t ts, bool forceKeyframe) {
    return impl_ && impl_->Encode(frame, ts, forceKeyframe);
}
bool NvencEncoder::SetBitrate(uint32_t bitrateBps) {
    return impl_ && impl_->SetBitrate(bitrateBps);
}
void NvencEncoder::Finish() { if (impl_) impl_->Finish(); }
