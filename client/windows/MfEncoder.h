#pragma once
//
// Backend encoder dung thang H.264/HEVC Encoder MFT (Media Foundation), khong qua
// IMFSinkWriter/container - de xuat duoc NAL Annex-B tho cho callback onPacket (streaming).
//
// Vi sao MF cho agent khong-NVIDIA (Intel QSV / AMD VCE / software fallback):
//   - Co san trong Windows SDK, khong can SDK hang thu ba.
//   - MFTEnumEx tu tim MFT phu hop device D3D11 dang dung (uu tien HW nho SORTANDFILTER).
//   - Dau vao NV12 tu chinh D3D11 device dang capture (VRAM) - tu chuyen mau BGRA->NV12
//     bang D3D11 Video Processor (khong dong bo CPU).
//   - MFT phan cung thuong BAT DONG BO (async) - phai unlock + bat su kien
//     (IMFMediaEventGenerator) truoc khi goi ProcessInput/ProcessOutput.
//   - SPS/PPS lay tu MF_MT_MPEG_SEQUENCE_HEADER cua kieu dau ra, tu chen truoc moi IDR
//     (tuong duong repeatSPSPPS cua NVENC) de client join/phuc hoi giua chung decode duoc.
//
#include "IVideoEncoder.h"

class MfEncoder : public IVideoEncoder {
public:
    MfEncoder();
    ~MfEncoder() override;

    bool Init(ID3D11Device* device, const EncoderConfig& cfg) override;
    bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) override;
    void Finish() override;
    const wchar_t* BackendName() const override { return L"Media Foundation (HW/SW auto)"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
