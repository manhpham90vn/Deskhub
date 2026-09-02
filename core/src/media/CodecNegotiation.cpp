#include "deskhub/media/CodecNegotiation.h"

namespace deskhub::media {
namespace {

constexpr Codec kPreference[] = {Codec::Av1, Codec::Hevc, Codec::H264High444, Codec::H264};

}

std::span<const Codec> CodecPreference() {
    return kPreference;
}

uint16_t CodecMaskOf(Codec codec) {
    switch (codec) {
        case Codec::H264: return kCodecMaskH264;
        case Codec::H264High444: return kCodecMaskH264High444;
        case Codec::Hevc: return kCodecMaskHevc;
        case Codec::Av1: return kCodecMaskAv1;
        case Codec::Rejected: return 0;
    }
    return 0;
}

const char* CodecName(Codec codec) {
    switch (codec) {
        case Codec::H264: return "H264";
        case Codec::H264High444: return "H264-4:4:4";
        case Codec::Hevc: return "HEVC";
        case Codec::Av1: return "AV1";
        case Codec::Rejected: return "rejected";
    }
    return "?";
}

bool CodecIsUniversal(Codec codec) {
    return codec == Codec::H264;
}

Codec NegotiateCodec(uint16_t hostMask, uint16_t clientMask) {
    const uint16_t shared = uint16_t(hostMask & clientMask);
    if (!shared) return Codec::Rejected;
    for (Codec candidate : kPreference)
        if (shared & CodecMaskOf(candidate)) return candidate;
    return Codec::Rejected;
}

}
