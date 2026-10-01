#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

#include "deskhub/media/AudioTypes.h"

namespace deskhub::media {

template <class E>
concept AudioEncoderLike = requires {
    static_cast<bool (E::*)(const AudioFormat&, uint32_t)>(&E::Open);
    static_cast<size_t (E::*)(std::span<const int16_t>, std::span<uint8_t>)>(&E::Encode);
    static_cast<void (E::*)()>(&E::Close);
    static_cast<bool (E::*)() const>(&E::IsOpen);
};

template <class D>
concept AudioDecoderLike = requires {
    static_cast<bool (D::*)(const AudioFormat&)>(&D::Open);
    static_cast<size_t (D::*)(std::span<const uint8_t>, std::span<int16_t>)>(&D::Decode);
    static_cast<size_t (D::*)(std::span<int16_t>)>(&D::Conceal);
    static_cast<size_t (D::*)(std::span<const uint8_t>, std::span<int16_t>)>(&D::Recover);
    static_cast<void (D::*)()>(&D::Close);
    static_cast<bool (D::*)() const>(&D::IsOpen);
};

}
