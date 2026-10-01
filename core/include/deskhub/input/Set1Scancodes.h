#pragma once
#include "deskhub/input/VirtualKeys.h"
#include "deskhub/protocol/Wire.h"

#include <cstdint>

namespace deskhub {

constexpr int32_t VkToSet1Scancode(int32_t vk) {
    if (vk >= '1' && vk <= '9') return 0x02 + (vk - '1');
    if (vk == '0') return 0x0B;
    if (vk >= kVkF1 && vk <= kVkF1 + 9) return 0x3B + (vk - kVkF1);
    if (vk == kVkF1 + 10) return 0x57;
    if (vk == kVkF1 + 11) return 0x58;
    if (vk >= kVkF1 + 12 && vk <= kVkF1 + 19) return 0x64 + (vk - kVkF1 - 12);

    switch (vk) {
        case 'A': return 0x1E;
        case 'B': return 0x30;
        case 'C': return 0x2E;
        case 'D': return 0x20;
        case 'E': return 0x12;
        case 'F': return 0x21;
        case 'G': return 0x22;
        case 'H': return 0x23;
        case 'I': return 0x17;
        case 'J': return 0x24;
        case 'K': return 0x25;
        case 'L': return 0x26;
        case 'M': return 0x32;
        case 'N': return 0x31;
        case 'O': return 0x18;
        case 'P': return 0x19;
        case 'Q': return 0x10;
        case 'R': return 0x13;
        case 'S': return 0x1F;
        case 'T': return 0x14;
        case 'U': return 0x16;
        case 'V': return 0x2F;
        case 'W': return 0x11;
        case 'X': return 0x2D;
        case 'Y': return 0x15;
        case 'Z': return 0x2C;

        case kVkEscape: return 0x01;
        case kVkOemMinus: return 0x0C;
        case kVkOemPlus: return 0x0D;
        case kVkBack: return 0x0E;
        case kVkTab: return 0x0F;
        case kVkOem4: return 0x1A;
        case kVkOem6: return 0x1B;
        case kVkReturn: return 0x1C;
        case kVkControl:
        case kVkLControl: return 0x1D;
        case kVkRControl: return 0x1D | kScanExtended;
        case kVkOem1: return 0x27;
        case kVkOem7: return 0x28;
        case kVkOem3: return 0x29;
        case kVkShift:
        case kVkLShift: return 0x2A;
        case kVkOem5: return 0x2B;
        case kVkOemComma: return 0x33;
        case kVkOemPeriod: return 0x34;
        case kVkOem2: return 0x35;
        case kVkRShift: return 0x36;
        case kVkMultiply: return 0x37;
        case kVkSnapshot: return 0x37 | kScanExtended;
        case kVkMenu:
        case kVkLMenu: return 0x38;
        case kVkRMenu: return 0x38 | kScanExtended;
        case kVkSpace: return 0x39;
        case kVkCapital: return 0x3A;
        case kVkNumLock: return 0x45;
        case kVkScroll: return 0x46;
        case kVkNumpad0 + 0: return 0x52;
        case kVkNumpad0 + 1: return 0x4F;
        case kVkNumpad0 + 2: return 0x50;
        case kVkNumpad0 + 3: return 0x51;
        case kVkNumpad0 + 4: return 0x4B;
        case kVkNumpad0 + 5: return 0x4C;
        case kVkNumpad0 + 6: return 0x4D;
        case kVkNumpad0 + 7: return 0x47;
        case kVkNumpad0 + 8: return 0x48;
        case kVkNumpad0 + 9: return 0x49;
        case kVkSubtract: return 0x4A;
        case kVkAdd: return 0x4E;
        case kVkDecimal: return 0x53;
        case kVkDivide: return 0x35 | kScanExtended;
        case kVkHome: return 0x47 | kScanExtended;
        case kVkUp: return 0x48 | kScanExtended;
        case kVkPrior: return 0x49 | kScanExtended;
        case kVkLeft: return 0x4B | kScanExtended;
        case kVkRight: return 0x4D | kScanExtended;
        case kVkEnd: return 0x4F | kScanExtended;
        case kVkDown: return 0x50 | kScanExtended;
        case kVkNext: return 0x51 | kScanExtended;
        case kVkInsert: return 0x52 | kScanExtended;
        case kVkDelete: return 0x53 | kScanExtended;
        case kVkLWin: return 0x5B | kScanExtended;
        case kVkRWin: return 0x5C | kScanExtended;
        case kVkApps: return 0x5D | kScanExtended;
        default: return 0;
    }
}

constexpr int32_t SidedModifier(int32_t vk, int32_t scan) {
    switch (vk) {
        case kVkShift:
            if (scan == VkToSet1Scancode(kVkRShift)) return kVkRShift;
            if (scan == VkToSet1Scancode(kVkLShift)) return kVkLShift;
            return vk;
        case kVkControl:
            if (scan == VkToSet1Scancode(kVkRControl)) return kVkRControl;
            if (scan == VkToSet1Scancode(kVkLControl)) return kVkLControl;
            return vk;
        case kVkMenu:
            if (scan == VkToSet1Scancode(kVkRMenu)) return kVkRMenu;
            if (scan == VkToSet1Scancode(kVkLMenu)) return kVkLMenu;
            return vk;
        default: return vk;
    }
}

constexpr bool NeedsVirtualKeyInjection(int32_t vk) {
    return vk == kVkPause;
}

}
