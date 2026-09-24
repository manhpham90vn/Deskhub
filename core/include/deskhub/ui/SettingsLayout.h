#pragma once
#include "deskhub/ui/UiSettings.h"

#include <cstdint>
#include <span>

namespace deskhub::ui {

inline constexpr int kSettingsAreaBarWidth = 4;

enum class SettingsEntryKind : uint8_t {
    Area,
    Hint,
    Section,
    Setting,
};

enum class SettingField : uint8_t {
    None,
    Fps,
    Bitrate,
    Quality,
    Passcode,
    AllowInput,
    ShareAudio,
    TransferFolder,
    AutoShare,
    Permissions,
    PlayAudio,
    Port,
    ClipboardSync,
    KeepAwake,
    Autostart,
    CloseToTray,
    Count,
};

struct SettingsEntry {
    SettingsEntryKind kind;
    const char* text;
    SettingField field;
};

std::span<const SettingsEntry> DesktopSettingsLayout();

bool* SettingFlag(UiSettings& settings, SettingField field);

}
