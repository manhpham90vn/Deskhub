#include "encode/VaEncoder.h"

#include <va/va_drmcommon.h>

#include <drm_fourcc.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "encode/VaDisplay.h"

#include "deskhub/media/BitWriter.h"
#include "deskhub/media/H264Encode.h"
#include "deskhub/media/RatePlan.h"

using deskhub::media::AlignEncodeSize;
using deskhub::media::BitWriter;
using deskhub::media::kH264MacroblockPx;
using deskhub::media::LevelFor;

namespace {

constexpr size_t kMinCodedBufSize = 1 << 20;

bool VaCheck(VAStatus st, const char* what) {
    if (st == VA_STATUS_SUCCESS) return true;
    LOGE("[VaEnc] %s failed: %s", what, vaErrorStr(st));
    return false;
}

const char* RateControlName(uint32_t vaRcMode) {
    switch (vaRcMode) {
        case VA_RC_CBR: return "CBR";
        case VA_RC_VBR: return "VBR";
        default: return "CQP";
    }
}

uint32_t DrmToVaFourcc(uint32_t drm) {
    switch (drm) {
        case DRM_FORMAT_XRGB8888: return VA_FOURCC_BGRX;
        case DRM_FORMAT_ARGB8888: return VA_FOURCC_BGRA;
        case DRM_FORMAT_XBGR8888: return VA_FOURCC_RGBX;
        case DRM_FORMAT_ABGR8888: return VA_FOURCC_RGBA;
        default: return 0;
    }
}

template <typename T>
VABufferID MakeMiscBuffer(VADisplay dpy, VAContextID ctx, VAEncMiscParameterType type, T** payload) {
    VABufferID id = VA_INVALID_ID;
    if (vaCreateBuffer(dpy, ctx, VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(T), 1, nullptr, &id) != VA_STATUS_SUCCESS)
        return VA_INVALID_ID;
    VAEncMiscParameterBuffer* hdr = nullptr;
    if (vaMapBuffer(dpy, id, reinterpret_cast<void**>(&hdr)) != VA_STATUS_SUCCESS) {
        vaDestroyBuffer(dpy, id);
        return VA_INVALID_ID;
    }
    hdr->type = type;
    *payload = reinterpret_cast<T*>(hdr->data);
    std::memset(*payload, 0, sizeof(T));
    return id;
}

VAPictureH264 InvalidPic() {
    VAPictureH264 p{};
    p.picture_id = VA_INVALID_SURFACE;
    p.flags = VA_PICTURE_H264_INVALID;
    return p;
}

constexpr int kQpMin = 16;
constexpr int kQpMax = 45;
constexpr int kQpStepMax = 1;
constexpr int kIdrQpDelta = 2;
constexpr double kEmaFrames = 15.0;
constexpr double kIdrSeconds = 0.25;
constexpr int kIdrQpJump = 6;
constexpr double kMaxBudgetFrames = 4.0;

constexpr uint32_t kVbrTargetPercentage = 70;
constexpr uint32_t kMinVbvWindowMs = 16;

}

VaEncoder::~VaEncoder() {
    Finish();
}

void VaEncoder::BuildParameterSets() {
    const uint8_t level = LevelFor(aligned_.mbWidth, aligned_.mbHeight, cfg_.fps);

    BitWriter w;
    w.StartNal(3, 7);
    w.U(8, 77);
    w.U(8, 0);
    w.U(8, level);
    w.UE(0);
    w.UE(kLog2MaxFrameNumMinus4);
    w.UE(0);
    w.UE(kLog2MaxPocLsbMinus4);
    w.UE(kMaxRefFrames);
    w.U(1, 0);
    w.UE(aligned_.mbWidth - 1);
    w.UE(aligned_.mbHeight - 1);
    w.U(1, 1);
    w.U(1, 1);
    w.U(1, aligned_.cropped ? 1 : 0);
    if (aligned_.cropped) {
        w.UE(0);
        w.UE(aligned_.cropRightOffset);
        w.UE(0);
        w.UE(aligned_.cropBottomOffset);
    }
    w.U(1, 1);
    {
        w.U(1, 0);
        w.U(1, 0);
        w.U(1, 1);
        w.U(3, 5);
        w.U(1, 0);
        w.U(1, 1);
        w.U(8, 1);
        w.U(8, 1);
        w.U(8, 1);
        w.U(1, 0);
        w.U(1, 1);
        w.U(32, 1);
        w.U(32, 2 * (cfg_.fps ? cfg_.fps : 60));
        w.U(1, 1);
        w.U(1, 0);
        w.U(1, 0);
        w.U(1, 0);
        w.U(1, 1);
        w.U(1, 1);
        w.UE(0);
        w.UE(0);
        w.UE(kLog2MaxMvLength);
        w.UE(kLog2MaxMvLength);
        w.UE(0);
        w.UE(kMaxRefFrames);
    }
    w.Trailing();
    sps_ = w.bytes();
    spsBits_ = w.bitLength();

    w.Clear();
    w.StartNal(3, 8);
    w.UE(0);
    w.UE(0);
    w.U(1, 1);
    w.U(1, 0);
    w.UE(0);
    w.UE(0);
    w.UE(0);
    w.U(1, 0);
    w.U(2, 0);
    w.SE(0);
    w.SE(0);
    w.SE(0);
    w.U(1, 1);
    w.U(1, 0);
    w.U(1, 0);
    w.Trailing();
    pps_ = w.bytes();
    ppsBits_ = w.bitLength();
}

bool VaEncoder::Init(const EncoderConfig& cfg) {
    Finish();

    if (!cfg.width || !cfg.height || (cfg.width & 1) || (cfg.height & 1)) {
        LOGE("[VaEnc] Refusing %ux%u — dimensions must be even and non-zero.", cfg.width,
            cfg.height);
        return false;
    }
    VaDisplay& vd = VaDisplay::Instance();
    if (!vd.Open()) return false;

    cfg_ = cfg;
    dpy_ = vd.handle();
    encEntrypoint_ = vd.encodeEntrypoint();
    aligned_ = AlignEncodeSize(cfg_.width, cfg_.height, kH264MacroblockPx);

    frameCount_ = 0;
    frameNum_ = 0;
    poc_ = 0;
    idrPicId_ = 0;
    haveRef_ = false;
    refSurface_ = VA_INVALID_SURFACE;
    pendingBitrate_ = 0;
    haveSource_ = false;
    qp_ = kPicInitQp;
    lastIdrQp_ = 0;
    lastIdrBytes_ = 0;
    logRatioEma_ = 0.0;
    haveRatio_ = false;
    lastEncodeUs_ = 0;

    BuildParameterSets();
    if (!CreateContexts()) {
        Finish();
        return false;
    }

    LOGI("[VaEnc] %ux%u (aligned %ux%u) @%u fps, %.1f Mbps %s%s — %s, packed headers %s.",
        cfg_.width, cfg_.height, aligned_.width, aligned_.height, cfg_.fps,
        cfg_.bitrateBps / 1e6, RateControlName(rcMode_), cfg_.lowLatency ? ", low latency" : "",
        vd.driverName().c_str(), packedHeaders_ ? "on" : "OFF (driver writes its own SPS/PPS)");
    return true;
}

bool VaEncoder::CreateContexts() {
    VAConfigAttrib attrs[3];
    attrs[0].type = VAConfigAttribRTFormat;
    attrs[1].type = VAConfigAttribRateControl;
    attrs[2].type = VAConfigAttribEncPackedHeaders;
    if (!VaCheck(vaGetConfigAttributes(dpy_, VAProfileH264Main, encEntrypoint_, attrs, 3),
            "vaGetConfigAttributes"))
        return false;

    if (!(attrs[0].value & VA_RT_FORMAT_YUV420)) {
        LOGE("[VaEnc] Driver does not support YUV420 encode surfaces.");
        return false;
    }
    const uint32_t offered =
        attrs[1].value == VA_ATTRIB_NOT_SUPPORTED ? 0u : uint32_t(attrs[1].value);
    const bool wantVbr = cfg_.rc == deskhub::media::RateControl::VBR;
    const uint32_t preferred = wantVbr ? VA_RC_VBR : VA_RC_CBR;
    const uint32_t fallback = wantVbr ? VA_RC_CBR : VA_RC_VBR;

    rcMode_ = VA_RC_CQP;
    if (offered & preferred)
        rcMode_ = preferred;
    else if (offered & fallback)
        rcMode_ = fallback;

    cqpMode_ = rcMode_ == VA_RC_CQP;
    if (cqpMode_)
        LOGI(
            "[VaEnc] Driver offers no %s — using constant QP with software rate control "
            "(QP %d..%d).",
            RateControlName(preferred), kQpMin, kQpMax);
    else if (rcMode_ != preferred)
        LOGI("[VaEnc] Driver offers no %s — falling back to %s.", RateControlName(preferred),
            RateControlName(rcMode_));

    packedHeaders_ = (attrs[2].value != VA_ATTRIB_NOT_SUPPORTED) &&
                     (attrs[2].value & VA_ENC_PACKED_HEADER_SEQUENCE) &&
                     (attrs[2].value & VA_ENC_PACKED_HEADER_PICTURE);

    VAConfigAttrib cfgAttrs[3];
    int nCfgAttrs = 0;
    cfgAttrs[nCfgAttrs].type = VAConfigAttribRTFormat;
    cfgAttrs[nCfgAttrs++].value = VA_RT_FORMAT_YUV420;
    cfgAttrs[nCfgAttrs].type = VAConfigAttribRateControl;
    cfgAttrs[nCfgAttrs++].value = rcMode_;
    if (packedHeaders_) {
        cfgAttrs[nCfgAttrs].type = VAConfigAttribEncPackedHeaders;
        cfgAttrs[nCfgAttrs++].value =
            VA_ENC_PACKED_HEADER_SEQUENCE | VA_ENC_PACKED_HEADER_PICTURE;
    }

    if (!VaCheck(vaCreateConfig(dpy_, VAProfileH264Main, encEntrypoint_, cfgAttrs, nCfgAttrs,
                     &encConfig_),
            "vaCreateConfig(encode)"))
        return false;

    VASurfaceID surfaces[3];
    if (!VaCheck(vaCreateSurfaces(dpy_, VA_RT_FORMAT_YUV420, aligned_.width, aligned_.height, surfaces, 3,
                     nullptr, 0),
            "vaCreateSurfaces(NV12)"))
        return false;
    srcNv12_ = surfaces[0];
    reconNv12_[0] = surfaces[1];
    reconNv12_[1] = surfaces[2];

    if (!VaCheck(vaCreateContext(dpy_, encConfig_, int(aligned_.width), int(aligned_.height),
                     VA_PROGRESSIVE, surfaces, 3, &encContext_),
            "vaCreateContext(encode)"))
        return false;

    const size_t codedSize =
        std::max(kMinCodedBufSize, size_t(aligned_.width) * aligned_.height * 3 / 2);
    if (!VaCheck(vaCreateBuffer(dpy_, encContext_, VAEncCodedBufferType, uint32_t(codedSize), 1,
                     nullptr, &codedBuf_),
            "vaCreateBuffer(coded)"))
        return false;

    if (!VaCheck(vaCreateConfig(dpy_, VAProfileNone, VAEntrypointVideoProc, nullptr, 0,
                     &vppConfig_),
            "vaCreateConfig(vpp)"))
        return false;
    if (!VaCheck(vaCreateContext(dpy_, vppConfig_, int(aligned_.width), int(aligned_.height), VA_PROGRESSIVE,
                     &srcNv12_, 1, &vppContext_),
            "vaCreateContext(vpp)"))
        return false;

    return true;
}

void VaEncoder::Finish() {
    if (!dpy_) return;

    if (haveRgbImage_) {
        vaDestroyImage(dpy_, rgbImage_.image_id);
        haveRgbImage_ = false;
    }
    if (rgbSurface_ != VA_INVALID_SURFACE) {
        vaDestroySurfaces(dpy_, &rgbSurface_, 1);
        rgbSurface_ = VA_INVALID_SURFACE;
    }
    if (codedBuf_ != VA_INVALID_ID) {
        vaDestroyBuffer(dpy_, codedBuf_);
        codedBuf_ = VA_INVALID_ID;
    }
    if (vppContext_ != VA_INVALID_ID) {
        vaDestroyContext(dpy_, vppContext_);
        vppContext_ = VA_INVALID_ID;
    }
    if (vppConfig_ != VA_INVALID_ID) {
        vaDestroyConfig(dpy_, vppConfig_);
        vppConfig_ = VA_INVALID_ID;
    }
    if (encContext_ != VA_INVALID_ID) {
        vaDestroyContext(dpy_, encContext_);
        encContext_ = VA_INVALID_ID;
    }
    if (encConfig_ != VA_INVALID_ID) {
        vaDestroyConfig(dpy_, encConfig_);
        encConfig_ = VA_INVALID_ID;
    }
    if (srcNv12_ != VA_INVALID_SURFACE) {
        VASurfaceID s[3] = {srcNv12_, reconNv12_[0], reconNv12_[1]};
        vaDestroySurfaces(dpy_, s, 3);
        srcNv12_ = reconNv12_[0] = reconNv12_[1] = VA_INVALID_SURFACE;
    }
    dpy_ = nullptr;
}

uint32_t VaEncoder::TargetPercentage() const {
    return rcMode_ == VA_RC_VBR ? kVbrTargetPercentage : 100;
}

uint32_t VaEncoder::PeakBitrate() const {
    return uint32_t(uint64_t(cfg_.bitrateBps) * 100 / TargetPercentage());
}

uint32_t VaEncoder::VbvWindowMs(const deskhub::media::RatePlan& plan) const {
    if (!cfg_.bitrateBps) return kMinVbvWindowMs;
    const uint32_t ms = uint32_t(uint64_t(plan.vbvBits) * 1000 / cfg_.bitrateBps);
    return std::max(ms, kMinVbvWindowMs);
}

int VaEncoder::IdrQp() const {
    const double budget = double(cfg_.bitrateBps) / 8.0 * kIdrSeconds;
    int q = qp_ + kIdrQpDelta;
    if (lastIdrBytes_ > 0 && budget > 1.0) {
        const int predicted = lastIdrQp_ +
                              int(std::lround(std::log(double(lastIdrBytes_) / budget) / std::log(1.125)));
        q = std::clamp(predicted, lastIdrQp_ - kIdrQpJump, lastIdrQp_ + kIdrQpJump);
    }
    return std::clamp(std::max(q, qp_ + kIdrQpDelta), kQpMin, kQpMax);
}

bool VaEncoder::SetBitrate(uint32_t bitrateBps) {
    if (!IsOpen() || bitrateBps < 100'000) return false;
    if (cqpMode_ && cfg_.bitrateBps && bitrateBps != cfg_.bitrateBps) {
        const int step = int(std::lround(
            std::log(double(cfg_.bitrateBps) / double(bitrateBps)) / std::log(1.125)));
        qp_ = std::clamp(qp_ + step, kQpMin, kQpMax);
        logRatioEma_ = 0.0;
        haveRatio_ = false;
    }
    cfg_.bitrateBps = bitrateBps;
    pendingBitrate_ = bitrateBps;
    return true;
}

VASurfaceID VaEncoder::ImportDmaBuf(const LinuxFrameInfo& fi) {
    const uint32_t vaFourcc = DrmToVaFourcc(fi.drmFormat);
    if (!vaFourcc || !fi.planeCount) return VA_INVALID_SURFACE;

    uint32_t objSize[kMaxDmaPlanes]{};
    for (uint32_t i = 0; i < fi.planeCount; ++i) {
        const off_t sz = lseek(fi.planes[i].fd, 0, SEEK_END);
        objSize[i] = sz > 0 ? uint32_t(sz) : fi.planes[i].offset + fi.planes[i].stride * fi.meta.height;
    }

    VASurfaceAttrib attrs[2]{};
    attrs[0].type = VASurfaceAttribMemoryType;
    attrs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrs[0].value.type = VAGenericValueTypeInteger;
    attrs[1].type = VASurfaceAttribExternalBufferDescriptor;
    attrs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attrs[1].value.type = VAGenericValueTypePointer;

    VADRMPRIMESurfaceDescriptor prime2{};
    VASurfaceAttribExternalBuffers legacy{};
    uintptr_t legacyFds[kMaxDmaPlanes]{};

    if (fi.modifier == DRM_FORMAT_MOD_INVALID) {
        legacy.pixel_format = vaFourcc;
        legacy.width = fi.meta.width;
        legacy.height = fi.meta.height;
        legacy.data_size = objSize[0];
        legacy.num_planes = fi.planeCount;
        for (uint32_t i = 0; i < fi.planeCount; ++i) {
            legacy.pitches[i] = fi.planes[i].stride;
            legacy.offsets[i] = fi.planes[i].offset;
            legacyFds[i] = uintptr_t(fi.planes[i].fd);
        }
        legacy.buffers = legacyFds;
        legacy.num_buffers = fi.planeCount;
        attrs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
        attrs[1].value.value.p = &legacy;
    } else {
        prime2.fourcc = vaFourcc;
        prime2.width = fi.meta.width;
        prime2.height = fi.meta.height;
        prime2.num_objects = fi.planeCount;
        for (uint32_t i = 0; i < fi.planeCount; ++i) {
            prime2.objects[i].fd = fi.planes[i].fd;
            prime2.objects[i].size = objSize[i];
            prime2.objects[i].drm_format_modifier = fi.modifier;
        }
        prime2.num_layers = 1;
        prime2.layers[0].drm_format = fi.drmFormat;
        prime2.layers[0].num_planes = fi.planeCount;
        for (uint32_t i = 0; i < fi.planeCount; ++i) {
            prime2.layers[0].object_index[i] = i;
            prime2.layers[0].offset[i] = fi.planes[i].offset;
            prime2.layers[0].pitch[i] = fi.planes[i].stride;
        }
        attrs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2;
        attrs[1].value.value.p = &prime2;
    }

    VASurfaceID surf = VA_INVALID_SURFACE;
    const VAStatus st = vaCreateSurfaces(dpy_, VA_RT_FORMAT_RGB32, fi.meta.width, fi.meta.height, &surf, 1,
        attrs, 2);
    if (st != VA_STATUS_SUCCESS) {
        return VA_INVALID_SURFACE;
    }
    return surf;
}

bool VaEncoder::UploadMapped(const LinuxFrameInfo& fi) {
    const uint32_t vaFourcc = DrmToVaFourcc(fi.drmFormat);
    if (!vaFourcc || !fi.handle || !fi.stride) return false;

    if (rgbFourcc_ != vaFourcc || rgbSurface_ == VA_INVALID_SURFACE) {
        if (haveRgbImage_) {
            vaDestroyImage(dpy_, rgbImage_.image_id);
            haveRgbImage_ = false;
        }
        if (rgbSurface_ != VA_INVALID_SURFACE) {
            vaDestroySurfaces(dpy_, &rgbSurface_, 1);
            rgbSurface_ = VA_INVALID_SURFACE;
        }

        VASurfaceAttrib attr{};
        attr.type = VASurfaceAttribPixelFormat;
        attr.flags = VA_SURFACE_ATTRIB_SETTABLE;
        attr.value.type = VAGenericValueTypeInteger;
        attr.value.value.i = int(vaFourcc);
        if (!VaCheck(vaCreateSurfaces(dpy_, VA_RT_FORMAT_RGB32, fi.meta.width, fi.meta.height, &rgbSurface_,
                         1, &attr, 1),
                "vaCreateSurfaces(RGB)"))
            return false;

        VAImageFormat fmt{};
        fmt.fourcc = vaFourcc;
        fmt.byte_order = VA_LSB_FIRST;
        fmt.bits_per_pixel = 32;
        fmt.depth = 24;
        const bool bgr = vaFourcc == VA_FOURCC_BGRX || vaFourcc == VA_FOURCC_BGRA;
        fmt.red_mask = bgr ? 0x00FF0000u : 0x000000FFu;
        fmt.green_mask = 0x0000FF00u;
        fmt.blue_mask = bgr ? 0x000000FFu : 0x00FF0000u;
        fmt.alpha_mask = 0xFF000000u;
        if (!VaCheck(vaCreateImage(dpy_, &fmt, int(fi.meta.width), int(fi.meta.height), &rgbImage_),
                "vaCreateImage(RGB)")) {
            vaDestroySurfaces(dpy_, &rgbSurface_, 1);
            rgbSurface_ = VA_INVALID_SURFACE;
            return false;
        }
        haveRgbImage_ = true;
        rgbFourcc_ = vaFourcc;
    }

    uint8_t* dst = nullptr;
    if (!VaCheck(vaMapBuffer(dpy_, rgbImage_.buf, reinterpret_cast<void**>(&dst)), "vaMapBuffer"))
        return false;
    const uint32_t dstPitch = rgbImage_.pitches[0];
    const uint32_t rowBytes = fi.meta.width * 4;
    const uint32_t copyBytes = rowBytes < dstPitch ? rowBytes : dstPitch;
    for (uint32_t y = 0; y < fi.meta.height; ++y)
        std::memcpy(dst + rgbImage_.offsets[0] + size_t(y) * dstPitch,
            fi.handle + size_t(y) * fi.stride, copyBytes);
    vaUnmapBuffer(dpy_, rgbImage_.buf);

    return VaCheck(vaPutImage(dpy_, rgbSurface_, rgbImage_.image_id, 0, 0, fi.meta.width, fi.meta.height, 0,
                       0, fi.meta.width, fi.meta.height),
        "vaPutImage");
}

bool VaEncoder::ConvertToNv12(VASurfaceID rgb) {
    const uint32_t sw = srcW_ ? srcW_ : cfg_.width;
    const uint32_t sh = srcH_ ? srcH_ : cfg_.height;
    VARectangle srcRect{0, 0, uint16_t(sw), uint16_t(sh)};
    VARectangle dstRect{0, 0, uint16_t(cfg_.width), uint16_t(cfg_.height)};

    VAProcPipelineParameterBuffer pp{};
    pp.surface = rgb;
    pp.surface_region = &srcRect;
    pp.output_region = &dstRect;
    pp.surface_color_standard = VAProcColorStandardNone;
    pp.output_color_standard = VAProcColorStandardBT709;
    pp.filter_flags = (sw != cfg_.width || sh != cfg_.height) ? VA_FILTER_SCALING_HQ
                                                              : VA_FILTER_SCALING_DEFAULT;

    VABufferID buf = VA_INVALID_ID;
    if (!VaCheck(vaCreateBuffer(dpy_, vppContext_, VAProcPipelineParameterBufferType, sizeof(pp),
                     1, &pp, &buf),
            "vaCreateBuffer(vpp)"))
        return false;

    bool ok = VaCheck(vaBeginPicture(dpy_, vppContext_, srcNv12_), "vaBeginPicture(vpp)");
    if (ok) ok = VaCheck(vaRenderPicture(dpy_, vppContext_, &buf, 1), "vaRenderPicture(vpp)");
    if (ok) ok = VaCheck(vaEndPicture(dpy_, vppContext_), "vaEndPicture(vpp)");
    vaDestroyBuffer(dpy_, buf);
    return ok;
}

bool VaEncoder::EncodeNv12(bool idr, size_t& outSize) {
    outSize = 0;

    const uint32_t slot = uint32_t(frameCount_ & 1);
    const VASurfaceID recon = reconNv12_[slot];

    if (idr) {
        frameNum_ = 0;
        poc_ = 0;
        haveRef_ = false;
        ++idrPicId_;
    }

    std::vector<VABufferID> pending;
    pending.reserve(8);
    auto push = [&](VABufferID id) {
        if (id != VA_INVALID_ID) pending.push_back(id);
    };
    auto cleanup = [&] {
        for (VABufferID id : pending) vaDestroyBuffer(dpy_, id);
    };

    if (!VaCheck(vaBeginPicture(dpy_, encContext_, srcNv12_), "vaBeginPicture(enc)")) return false;

    if (idr) {
        VAEncSequenceParameterBufferH264 seq{};
        seq.seq_parameter_set_id = 0;
        seq.level_idc = LevelFor(aligned_.mbWidth, aligned_.mbHeight, cfg_.fps);
        seq.intra_period = 0;
        seq.intra_idr_period = 0;
        seq.ip_period = 1;
        seq.bits_per_second = PeakBitrate();
        seq.max_num_ref_frames = kMaxRefFrames;
        seq.picture_width_in_mbs = uint16_t(aligned_.mbWidth);
        seq.picture_height_in_mbs = uint16_t(aligned_.mbHeight);
        seq.seq_fields.bits.chroma_format_idc = 1;
        seq.seq_fields.bits.frame_mbs_only_flag = 1;
        seq.seq_fields.bits.direct_8x8_inference_flag = 1;
        seq.seq_fields.bits.log2_max_frame_num_minus4 = kLog2MaxFrameNumMinus4;
        seq.seq_fields.bits.pic_order_cnt_type = 0;
        seq.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4 = kLog2MaxPocLsbMinus4;
        seq.frame_cropping_flag = aligned_.cropped ? 1 : 0;
        seq.frame_crop_right_offset = aligned_.cropRightOffset;
        seq.frame_crop_bottom_offset = aligned_.cropBottomOffset;
        seq.vui_parameters_present_flag = 1;
        seq.vui_fields.bits.timing_info_present_flag = 1;
        seq.vui_fields.bits.fixed_frame_rate_flag = 1;
        seq.num_units_in_tick = 1;
        seq.time_scale = 2 * (cfg_.fps ? cfg_.fps : 60);

        VABufferID id = VA_INVALID_ID;
        if (VaCheck(vaCreateBuffer(dpy_, encContext_, VAEncSequenceParameterBufferType,
                        sizeof(seq), 1, &seq, &id),
                "vaCreateBuffer(seq)")) {
            push(id);
            if (!VaCheck(vaRenderPicture(dpy_, encContext_, &id, 1), "vaRenderPicture(seq)")) {
                cleanup();
                vaEndPicture(dpy_, encContext_);
                return false;
            }
        }
    }

    if (idr || pendingBitrate_) {
        const deskhub::media::RatePlan plan =
            deskhub::media::PlanRateControl(cfg_.bitrateBps, cfg_.fps, cfg_.lowLatency);

        VAEncMiscParameterRateControl* rc = nullptr;
        VABufferID id = MakeMiscBuffer(dpy_, encContext_, VAEncMiscParameterTypeRateControl, &rc);
        if (id != VA_INVALID_ID) {
            rc->bits_per_second = PeakBitrate();
            rc->target_percentage = TargetPercentage();
            rc->window_size = VbvWindowMs(plan);
            rc->initial_qp = 0;
            rc->min_qp = 0;
            rc->max_qp = 0;
            rc->rc_flags.bits.reset = pendingBitrate_ ? 1 : 0;
            rc->rc_flags.bits.disable_frame_skip = 1;
            vaUnmapBuffer(dpy_, id);
            push(id);
            vaRenderPicture(dpy_, encContext_, &id, 1);
        }

        VAEncMiscParameterHRD* hrd = nullptr;
        VABufferID hrdId = MakeMiscBuffer(dpy_, encContext_, VAEncMiscParameterTypeHRD, &hrd);
        if (hrdId != VA_INVALID_ID) {
            hrd->buffer_size = plan.vbvBits;
            hrd->initial_buffer_fullness = plan.vbvInitialBits;
            vaUnmapBuffer(dpy_, hrdId);
            push(hrdId);
            vaRenderPicture(dpy_, encContext_, &hrdId, 1);
        }

        VAEncMiscParameterFrameRate* fr = nullptr;
        VABufferID frId = MakeMiscBuffer(dpy_, encContext_, VAEncMiscParameterTypeFrameRate, &fr);
        if (frId != VA_INVALID_ID) {
            fr->framerate = plan.fps;
            vaUnmapBuffer(dpy_, frId);
            push(frId);
            vaRenderPicture(dpy_, encContext_, &frId, 1);
        }
        pendingBitrate_ = 0;
    }

    if (idr && packedHeaders_) {
        struct {
            VAEncPackedHeaderType type;
            const std::vector<uint8_t>* data;
            uint32_t bits;
        } headers[2] = {
            {VAEncPackedHeaderSequence, &sps_, spsBits_},
            {VAEncPackedHeaderPicture, &pps_, ppsBits_},
        };
        for (const auto& h : headers) {
            VAEncPackedHeaderParameterBuffer ph{};
            ph.type = h.type;
            ph.bit_length = h.bits;
            ph.has_emulation_bytes = 1;

            VABufferID pid = VA_INVALID_ID, did = VA_INVALID_ID;
            if (!VaCheck(vaCreateBuffer(dpy_, encContext_, VAEncPackedHeaderParameterBufferType,
                             sizeof(ph), 1, &ph, &pid),
                    "vaCreateBuffer(packed hdr param)"))
                continue;
            if (!VaCheck(vaCreateBuffer(dpy_, encContext_, VAEncPackedHeaderDataBufferType,
                             uint32_t(h.data->size()), 1,
                             const_cast<uint8_t*>(h.data->data()), &did),
                    "vaCreateBuffer(packed hdr data)")) {
                vaDestroyBuffer(dpy_, pid);
                continue;
            }
            push(pid);
            push(did);
            VABufferID both[2] = {pid, did};
            vaRenderPicture(dpy_, encContext_, both, 2);
        }
    }

    VAEncPictureParameterBufferH264 pic{};
    pic.CurrPic.picture_id = recon;
    pic.CurrPic.frame_idx = frameNum_;
    pic.CurrPic.flags = 0;
    pic.CurrPic.TopFieldOrderCnt = poc_;
    pic.CurrPic.BottomFieldOrderCnt = poc_;
    for (auto& r : pic.ReferenceFrames) r = InvalidPic();
    if (haveRef_) {
        pic.ReferenceFrames[0].picture_id = refSurface_;
        pic.ReferenceFrames[0].frame_idx = refFrameNum_;
        pic.ReferenceFrames[0].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pic.ReferenceFrames[0].TopFieldOrderCnt = refPoc_;
        pic.ReferenceFrames[0].BottomFieldOrderCnt = refPoc_;
    }
    pic.coded_buf = codedBuf_;
    pic.pic_parameter_set_id = 0;
    pic.seq_parameter_set_id = 0;
    pic.last_picture = 0;
    pic.frame_num = uint16_t(frameNum_);
    pic.pic_init_qp = 26;
    pic.num_ref_idx_l0_active_minus1 = 0;
    pic.num_ref_idx_l1_active_minus1 = 0;
    pic.chroma_qp_index_offset = 0;
    pic.second_chroma_qp_index_offset = 0;
    pic.pic_fields.bits.idr_pic_flag = idr ? 1 : 0;
    pic.pic_fields.bits.reference_pic_flag = 1;
    pic.pic_fields.bits.entropy_coding_mode_flag = 1;
    pic.pic_fields.bits.deblocking_filter_control_present_flag = 1;
    pic.pic_fields.bits.transform_8x8_mode_flag = 0;

    {
        VABufferID id = VA_INVALID_ID;
        if (!VaCheck(vaCreateBuffer(dpy_, encContext_, VAEncPictureParameterBufferType,
                         sizeof(pic), 1, &pic, &id),
                "vaCreateBuffer(pic)")) {
            cleanup();
            vaEndPicture(dpy_, encContext_);
            return false;
        }
        push(id);
        if (!VaCheck(vaRenderPicture(dpy_, encContext_, &id, 1), "vaRenderPicture(pic)")) {
            cleanup();
            vaEndPicture(dpy_, encContext_);
            return false;
        }
    }

    VAEncSliceParameterBufferH264 slice{};
    slice.macroblock_address = 0;
    slice.num_macroblocks = aligned_.mbWidth * aligned_.mbHeight;
    slice.macroblock_info = VA_INVALID_ID;
    slice.slice_type = idr ? 2 : 0;
    slice.pic_parameter_set_id = 0;
    slice.idr_pic_id = idrPicId_;
    slice.pic_order_cnt_lsb = uint16_t(poc_ & ((1 << (kLog2MaxPocLsbMinus4 + 4)) - 1));
    slice.num_ref_idx_active_override_flag = 0;
    slice.num_ref_idx_l0_active_minus1 = 0;
    slice.num_ref_idx_l1_active_minus1 = 0;
    for (auto& r : slice.RefPicList0) r = InvalidPic();
    for (auto& r : slice.RefPicList1) r = InvalidPic();
    if (!idr && haveRef_) slice.RefPicList0[0] = pic.ReferenceFrames[0];
    slice.cabac_init_idc = 0;
    const int frameQp = idr ? IdrQp() : std::clamp(qp_, kQpMin, kQpMax);
    slice.slice_qp_delta = cqpMode_ ? int8_t(frameQp - kPicInitQp) : 0;
    slice.disable_deblocking_filter_idc = 0;

    {
        VABufferID id = VA_INVALID_ID;
        if (!VaCheck(vaCreateBuffer(dpy_, encContext_, VAEncSliceParameterBufferType,
                         sizeof(slice), 1, &slice, &id),
                "vaCreateBuffer(slice)")) {
            cleanup();
            vaEndPicture(dpy_, encContext_);
            return false;
        }
        push(id);
        if (!VaCheck(vaRenderPicture(dpy_, encContext_, &id, 1), "vaRenderPicture(slice)")) {
            cleanup();
            vaEndPicture(dpy_, encContext_);
            return false;
        }
    }

    const bool ended = VaCheck(vaEndPicture(dpy_, encContext_), "vaEndPicture(enc)");
    cleanup();
    if (!ended) return false;

    if (!VaCheck(vaSyncSurface(dpy_, srcNv12_), "vaSyncSurface")) return false;

    VACodedBufferSegment* seg = nullptr;
    if (!VaCheck(vaMapBuffer(dpy_, codedBuf_, reinterpret_cast<void**>(&seg)), "vaMapBuffer(coded)"))
        return false;
    out_.clear();
    bool overflow = false;
    for (VACodedBufferSegment* s = seg; s; s = static_cast<VACodedBufferSegment*>(s->next)) {
        if (s->status & VA_CODED_BUF_STATUS_SLICE_OVERFLOW_MASK) overflow = true;
        const auto* p = static_cast<const uint8_t*>(s->buf);
        out_.insert(out_.end(), p, p + s->size);
    }
    vaUnmapBuffer(dpy_, codedBuf_);

    if (overflow) {
        LOGW("[VaEnc] Coded buffer overflowed — frame dropped.");
        return false;
    }
    if (out_.empty()) return false;

    if (cqpMode_) {
        const double fps = double(cfg_.fps ? cfg_.fps : 60);
        const uint64_t nowUs = NowUs();
        const double frameUs = 1e6 / fps;
        const double elapsedUs = lastEncodeUs_
                                     ? std::clamp(double(nowUs - lastEncodeUs_), 0.0, frameUs * kMaxBudgetFrames)
                                     : frameUs;
        lastEncodeUs_ = nowUs;
        const double budget = double(cfg_.bitrateBps) / 8.0 * elapsedUs / 1e6;

        if (idr) {
            lastIdrQp_ = frameQp;
            lastIdrBytes_ = out_.size();
        }
        if (!idr && budget > 1.0) {
            const double logRatio = std::log(double(out_.size() ? out_.size() : 1) / budget);
            const double a = 1.0 / kEmaFrames;
            logRatioEma_ = haveRatio_ ? logRatioEma_ * (1.0 - a) + logRatio * a : logRatio;
            haveRatio_ = true;
            const int step = std::clamp(int(std::lround(logRatioEma_ / std::log(1.125))),
                -kQpStepMax, kQpStepMax);
            qp_ = std::clamp(qp_ + step, kQpMin, kQpMax);
        }
    }

    refSurface_ = recon;
    refFrameNum_ = frameNum_;
    refPoc_ = poc_;
    haveRef_ = true;
    frameNum_ = (frameNum_ + 1) & ((1u << (kLog2MaxFrameNumMinus4 + 4)) - 1);
    poc_ += 2;
    ++frameCount_;

    outSize = out_.size();
    return true;
}

bool VaEncoder::Encode(const LinuxFrameInfo& fi, uint64_t timestampUs, bool forceKeyframe) {
    if (!IsOpen()) return false;
    if (fi.meta.width < cfg_.width || fi.meta.height < cfg_.height) {
        static thread_local bool warned = false;
        if (!warned) {
            warned = true;
            LOGE(
                "[VaEnc] Frame is %ux%u but the encoder was built for %ux%u — dropping every "
                "frame until it is rebuilt.",
                fi.meta.width, fi.meta.height, cfg_.width, cfg_.height);
        }
        return false;
    }
    srcW_ = fi.meta.width;
    srcH_ = fi.meta.height;

    const bool idr = forceKeyframe || !haveRef_;

    VASurfaceID rgb = VA_INVALID_SURFACE;
    bool imported = false;
    if (fi.memory == FrameMemory::DmaBuf) {
        rgb = ImportDmaBuf(fi);
        imported = rgb != VA_INVALID_SURFACE;
        if (!imported) {
            static thread_local bool warned = false;
            if (!warned) {
                warned = true;
                LOGE(
                    "[VaEnc] Cannot import the compositor's dma-buf (modifier 0x%llx). "
                    "The GPU driver and the compositor disagree on buffer layout.",
                    (unsigned long long)fi.modifier);
            }
            return false;
        }
    } else {
        if (!UploadMapped(fi)) return false;
        rgb = rgbSurface_;
    }

    bool ok = ConvertToNv12(rgb);
    if (ok) haveSource_ = true;
    size_t size = 0;
    if (ok) ok = EncodeNv12(idr, size);

    if (imported) vaDestroySurfaces(dpy_, &rgb, 1);

    if (ok && cfg_.onPacket) cfg_.onPacket(out_.data(), size, timestampUs, idr);
    return ok;
}

bool VaEncoder::EncodeLast(uint64_t timestampUs, bool forceKeyframe) {
    if (!IsOpen() || !haveSource_) return false;
    const bool idr = forceKeyframe || !haveRef_;
    size_t size = 0;
    if (!EncodeNv12(idr, size)) return false;
    if (cfg_.onPacket) cfg_.onPacket(out_.data(), size, timestampUs, idr);
    return true;
}
