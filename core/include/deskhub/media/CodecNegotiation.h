#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <span>

namespace deskhub::media {

std::span<const Codec> CodecPreference();

uint16_t CodecMaskOf(Codec codec);

const char* CodecName(Codec codec);

bool CodecIsUniversal(Codec codec);

Codec NegotiateCodec(uint16_t hostMask, uint16_t clientMask);

}
