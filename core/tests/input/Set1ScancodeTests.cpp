#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/input/ScancodeTable.h"
#include "deskhub/input/Set1Scancodes.h"
#include "deskhub/input/VirtualKeys.h"

#include <cstdio>
#include <map>
#include <vector>

using namespace deskhub;

namespace {

constexpr int32_t kBase = kScanExtended - 1;

bool IsExtended(int32_t scan) {
    return (scan & kScanExtended) != 0;
}

void TestEveryMappedKeyProducesALegalSet1Code() {
    std::printf("[set1] a mapped key never yields a code the wire cannot carry...\n");
    for (int32_t vk = 0; vk <= 0xFF; ++vk) {
        const int32_t scan = VkToSet1Scancode(vk);
        if (!scan) continue;
        Check((scan & ~kScanExtended) >= 0x01 && (scan & ~kScanExtended) <= 0x7F,
            "the base code stays inside the one-byte make-code range");
        Check((scan & ~(kBase | kScanExtended)) == 0,
            "no bits outside the base code and the extended flag are ever set");
    }
}

void TestTheTypingKeysAreAllThere() {
    std::printf("[set1] every key a user types on has a scancode...\n");
    for (int32_t vk = 'A'; vk <= 'Z'; ++vk)
        Check(VkToSet1Scancode(vk) != 0, "every letter maps");
    for (int32_t vk = '0'; vk <= '9'; ++vk)
        Check(VkToSet1Scancode(vk) != 0, "every digit maps");
    for (int32_t i = 0; i < 20; ++i)
        Check(VkToSet1Scancode(kVkF1 + i) != 0, "F1 through F20 map");
    for (int32_t i = 0; i < 10; ++i)
        Check(VkToSet1Scancode(kVkNumpad0 + i) != 0, "every numpad digit maps");

    const int32_t oem[] = {kVkOem1, kVkOem2, kVkOem3, kVkOem4, kVkOem5, kVkOem6, kVkOem7,
        kVkOemPlus, kVkOemMinus, kVkOemComma, kVkOemPeriod};
    for (int32_t vk : oem) Check(VkToSet1Scancode(vk) != 0, "every punctuation key maps");
}

void TestDigitsAndLettersRunInTheRightOrder() {
    std::printf("[set1] the number row and the function row climb one code at a time...\n");
    for (int32_t i = 0; i < 9; ++i)
        Check(VkToSet1Scancode('1' + i) == 0x02 + i, "'1'..'9' are contiguous from 0x02");
    Check(VkToSet1Scancode('0') == 0x0B, "'0' sits after '9', as on the keyboard");

    for (int32_t i = 0; i < 10; ++i)
        Check(VkToSet1Scancode(kVkF1 + i) == 0x3B + i, "F1..F10 are contiguous from 0x3B");
    for (int32_t i = 0; i < 8; ++i)
        Check(VkToSet1Scancode(kVkF1 + 12 + i) == 0x64 + i, "F13..F20 are contiguous from 0x64");
}

void TestOnlyTheGreyKeysAreExtended() {
    std::printf("[set1] the extended flag marks exactly the duplicated grey keys...\n");
    const int32_t extended[] = {kVkRControl, kVkRMenu, kVkSnapshot, kVkDivide, kVkHome, kVkUp,
        kVkPrior, kVkLeft, kVkRight, kVkEnd, kVkDown, kVkNext, kVkInsert, kVkDelete, kVkLWin,
        kVkRWin, kVkApps};
    for (int32_t vk : extended)
        Check(IsExtended(VkToSet1Scancode(vk)), "grey/right-hand keys carry the extended flag");

    const int32_t plain[] = {'A', '1', kVkSpace, kVkReturn, kVkTab, kVkBack, kVkEscape,
        kVkLShift, kVkRShift, kVkLControl, kVkLMenu, kVkCapital, kVkNumLock, kVkAdd,
        kVkSubtract, kVkMultiply, kVkDecimal};
    for (int32_t vk : plain)
        Check(!IsExtended(VkToSet1Scancode(vk)), "the main block is never marked extended");

    for (int32_t i = 0; i < 10; ++i)
        Check(!IsExtended(VkToSet1Scancode(kVkNumpad0 + i)),
            "numpad digits are plain, which is what tells them apart from the arrows");
}

void TestTheNavigationClusterShadowsTheNumpad() {
    std::printf("[set1] arrows and numpad share a base code and differ only by the flag...\n");
    const struct {
        int32_t nav;
        int32_t pad;
    } pairs[] = {{kVkHome, kVkNumpad0 + 7}, {kVkUp, kVkNumpad0 + 8}, {kVkPrior, kVkNumpad0 + 9},
        {kVkLeft, kVkNumpad0 + 4}, {kVkRight, kVkNumpad0 + 6}, {kVkEnd, kVkNumpad0 + 1},
        {kVkDown, kVkNumpad0 + 2}, {kVkNext, kVkNumpad0 + 3}, {kVkInsert, kVkNumpad0 + 0},
        {kVkDelete, kVkDecimal}};
    for (const auto& p : pairs)
        Check(VkToSet1Scancode(p.nav) == (VkToSet1Scancode(p.pad) | kScanExtended),
            "the navigation key is the numpad key plus the extended flag, as on real hardware");
}

void TestNoTwoTypingKeysCollide() {
    std::printf("[set1] two different keys never arrive at the host as the same key...\n");
    std::vector<int32_t> keys;
    for (int32_t vk = 'A'; vk <= 'Z'; ++vk) keys.push_back(vk);
    for (int32_t vk = '0'; vk <= '9'; ++vk) keys.push_back(vk);
    for (int32_t i = 0; i < 20; ++i) keys.push_back(kVkF1 + i);
    for (int32_t i = 0; i < 10; ++i) keys.push_back(kVkNumpad0 + i);
    const int32_t rest[] = {kVkOem1, kVkOem2, kVkOem3, kVkOem4, kVkOem5, kVkOem6, kVkOem7,
        kVkOemPlus, kVkOemMinus, kVkOemComma, kVkOemPeriod, kVkEscape, kVkBack, kVkTab,
        kVkReturn, kVkSpace, kVkCapital, kVkNumLock, kVkScroll, kVkSnapshot, kVkAdd,
        kVkSubtract, kVkMultiply, kVkDivide, kVkDecimal, kVkHome, kVkEnd, kVkPrior, kVkNext,
        kVkInsert, kVkDelete, kVkUp, kVkDown, kVkLeft, kVkRight, kVkLShift, kVkRShift,
        kVkLControl, kVkRControl, kVkLMenu, kVkRMenu, kVkLWin, kVkRWin, kVkApps};
    for (int32_t vk : rest) keys.push_back(vk);

    std::map<int32_t, int32_t> byScan;
    for (int32_t vk : keys) {
        const int32_t scan = VkToSet1Scancode(vk);
        const auto it = byScan.find(scan);
        if (it != byScan.end()) {
            std::printf("  collision: vk 0x%02X and vk 0x%02X both send scan 0x%03X\n",
                unsigned(it->second), unsigned(vk), unsigned(scan));
            Check(false, "each key on the physical keyboard owns its scancode");
            continue;
        }
        byScan.emplace(scan, vk);
    }
}

void TestGenericModifiersResolveToTheLeftHandKey() {
    std::printf("[set1] a side-less modifier is sent as the left one, not dropped...\n");
    Check(VkToSet1Scancode(kVkShift) == VkToSet1Scancode(kVkLShift), "shift");
    Check(VkToSet1Scancode(kVkControl) == VkToSet1Scancode(kVkLControl), "control");
    Check(VkToSet1Scancode(kVkMenu) == VkToSet1Scancode(kVkLMenu), "alt");

    Check(PreferLeftModifier(kVkShift) == kVkLShift, "shift resolves left");
    Check(PreferLeftModifier(kVkControl) == kVkLControl, "control resolves left");
    Check(PreferLeftModifier(kVkMenu) == kVkLMenu, "alt resolves left");
    Check(PreferLeftModifier(kVkRShift) == kVkRShift, "a key that already names a side is kept");
    Check(PreferLeftModifier('A') == 'A', "an ordinary key is untouched");
}

void TestPauseIsNeverSentAsAOneByteScancode() {
    std::printf("[set1] Pause has no one-byte make code, so it is not faked as one...\n");
    Check(VkToSet1Scancode(kVkPause) == 0,
        "Pause is E1 1D 45 on the wire; any single byte we invented would be another key");
    Check(VkToSet1Scancode(kVkPause) != VkToSet1Scancode(kVkNumLock),
        "in particular it must not resolve to 0x45, which would toggle NumLock on the host");
    Check(NeedsVirtualKeyInjection(kVkPause),
        "so it is flagged for injection by virtual key instead");

    const int32_t ordinary[] = {'A', kVkNumLock, kVkScroll, kVkSnapshot, kVkEscape, kVkUp,
        kVkLControl, kVkF1};
    for (int32_t vk : ordinary)
        Check(!NeedsVirtualKeyInjection(vk),
            "every key that does have a scancode still goes the scancode route");
}

void TestUnmappedKeysAreRejectedNotGuessed() {
    std::printf("[set1] a key we have no code for sends nothing rather than a wrong key...\n");
    const int32_t unmapped[] = {0x00, 0x01, 0x07, 0x0A, 0x1F, 0x3A, 0x88, 0xFF};
    for (int32_t vk : unmapped)
        Check(VkToSet1Scancode(vk) == 0, "an unmapped vk yields no scancode at all");
}

void TestModifierClassification() {
    std::printf("[set1] the modifier a key belongs to does not depend on its side...\n");
    Check(ModifierKeyOf(kVkShift) == ModifierKey::Shift &&
              ModifierKeyOf(kVkLShift) == ModifierKey::Shift &&
              ModifierKeyOf(kVkRShift) == ModifierKey::Shift,
        "all three shift keys are shift");
    Check(ModifierKeyOf(kVkControl) == ModifierKey::Control &&
              ModifierKeyOf(kVkLControl) == ModifierKey::Control &&
              ModifierKeyOf(kVkRControl) == ModifierKey::Control,
        "all three control keys are control");
    Check(ModifierKeyOf(kVkMenu) == ModifierKey::Menu &&
              ModifierKeyOf(kVkLMenu) == ModifierKey::Menu &&
              ModifierKeyOf(kVkRMenu) == ModifierKey::Menu,
        "all three alt keys are alt");
    Check(ModifierKeyOf(kVkLWin) == ModifierKey::Win && ModifierKeyOf(kVkRWin) == ModifierKey::Win,
        "both win keys are win");
    Check(ModifierKeyOf(kVkCapital) == ModifierKey::CapsLock, "caps lock is its own modifier");

    const int32_t ordinary[] = {'A', '0', kVkSpace, kVkReturn, kVkF1, kVkNumLock, kVkApps,
        kVkEscape};
    for (int32_t vk : ordinary)
        Check(ModifierKeyOf(vk) == ModifierKey::None, "an ordinary key is not a modifier");
}

void TestTheScancodeSaysWhichSideAModifierIsOn() {
    std::printf("[set1] a generic modifier keeps its side when the scancode names it...\n");
    Check(SidedModifier(kVkMenu, 0x38 | kScanExtended) == kVkRMenu,
        "right Alt, which is AltGr on many layouts, stays right Alt");
    Check(SidedModifier(kVkControl, 0x1D | kScanExtended) == kVkRControl, "right Ctrl stays right");
    Check(SidedModifier(kVkShift, 0x36) == kVkRShift, "right Shift stays right");
    Check(SidedModifier(kVkMenu, 0x38) == kVkLMenu && SidedModifier(kVkControl, 0x1D) == kVkLControl &&
              SidedModifier(kVkShift, 0x2A) == kVkLShift,
        "and the left-hand keys stay left");
    Check(SidedModifier(kVkShift, 0) == kVkShift, "with no scancode the key stays generic");
    Check(SidedModifier('A', 0x36) == 'A', "and an ordinary key is never turned into a modifier");
}

}

void RunSet1ScancodeTests() {
    TestEveryMappedKeyProducesALegalSet1Code();
    TestTheTypingKeysAreAllThere();
    TestDigitsAndLettersRunInTheRightOrder();
    TestOnlyTheGreyKeysAreExtended();
    TestTheNavigationClusterShadowsTheNumpad();
    TestNoTwoTypingKeysCollide();
    TestGenericModifiersResolveToTheLeftHandKey();
    TestPauseIsNeverSentAsAOneByteScancode();
    TestUnmappedKeysAreRejectedNotGuessed();
    TestModifierClassification();
    TestTheScancodeSaysWhichSideAModifierIsOn();
}
