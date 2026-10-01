#pragma once
#include <cstdint>

#include "deskhub/protocol/Wire.h"

namespace deskhub {

inline constexpr uint32_t kX11ButtonLeft = 1;
inline constexpr uint32_t kX11ButtonMiddle = 2;
inline constexpr uint32_t kX11ButtonRight = 3;
inline constexpr uint32_t kX11ButtonBack = 8;
inline constexpr uint32_t kX11ButtonForward = 9;

constexpr bool X11ButtonToMouseButton(uint32_t x11Button, MouseButton& out) {
    switch (x11Button) {
        case kX11ButtonLeft: out = MouseButton::Left; return true;
        case kX11ButtonMiddle: out = MouseButton::Middle; return true;
        case kX11ButtonRight: out = MouseButton::Right; return true;
        case kX11ButtonBack: out = MouseButton::X1; return true;
        case kX11ButtonForward: out = MouseButton::X2; return true;
        default: return false;
    }
}

inline constexpr int32_t kAbsCoordMax = 65535;
inline constexpr int32_t kWheelDeltaPerNotch = 120;

constexpr int32_t ClampAbsCoord(int32_t normalized) {
    if (normalized < 0) return 0;
    if (normalized > kAbsCoordMax) return kAbsCoordMax;
    return normalized;
}

constexpr double AbsCoordToAxis(int32_t normalized, double origin, double extent) {
    return origin + double(ClampAbsCoord(normalized)) / double(kAbsCoordMax) * extent;
}

constexpr int64_t AbsCoordToPixel(int32_t normalized, int64_t origin, int64_t pixels) {
    if (pixels <= 1) return origin;
    const int64_t scaled = int64_t(ClampAbsCoord(normalized)) * (pixels - 1) + kAbsCoordMax / 2;
    return origin + scaled / kAbsCoordMax;
}

constexpr int32_t AxisToAbsCoord(int64_t pixel, int64_t origin, int64_t pixels) {
    if (pixels <= 1) return 0;
    const int64_t offset = pixel - origin;
    if (offset <= 0) return 0;
    const int64_t scaled = offset * kAbsCoordMax / (pixels - 1);
    return scaled >= kAbsCoordMax ? kAbsCoordMax : int32_t(scaled);
}

constexpr int32_t TakeWheelNotches(int32_t delta, int32_t& carry) {
    if ((delta > 0 && carry < 0) || (delta < 0 && carry > 0)) carry = 0;
    carry += delta;
    const int32_t whole = carry / kWheelDeltaPerNotch;
    carry -= whole * kWheelDeltaPerNotch;
    return whole;
}

constexpr int32_t ScrollNotchesFromLines(double lines) {
    if (lines == 0) return 0;
    const double magnitude = lines < 0 ? -lines : lines;
    int32_t notches = int32_t(magnitude);
    if (magnitude - double(notches) >= 0.5) ++notches;
    if (notches < 1) notches = 1;
    return lines < 0 ? -notches : notches;
}

inline constexpr double kTouchPointsPerNotch = 40.0;

constexpr int32_t TakeScrollNotches(double dragPoints, double& carry) {
    carry += dragPoints;
    const double whole = carry < 0 ? -double(int64_t(-carry / kTouchPointsPerNotch))
                                   : double(int64_t(carry / kTouchPointsPerNotch));
    carry -= whole * kTouchPointsPerNotch;
    return int32_t(whole);
}

}
