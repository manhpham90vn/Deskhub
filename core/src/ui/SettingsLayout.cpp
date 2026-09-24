#include "deskhub/ui/SettingsLayout.h"

#include "deskhub/ui/Strings.h"

namespace deskhub::ui {

namespace {

constexpr SettingsEntry Area(const char* title) {
    return SettingsEntry{SettingsEntryKind::Area, title, SettingField::None};
}

constexpr SettingsEntry Hint(const char* text) {
    return SettingsEntry{SettingsEntryKind::Hint, text, SettingField::None};
}

constexpr SettingsEntry Section(const char* title) {
    return SettingsEntry{SettingsEntryKind::Section, title, SettingField::None};
}

constexpr SettingsEntry Setting(SettingField field, const char* label) {
    return SettingsEntry{SettingsEntryKind::Setting, label, field};
}

constexpr SettingsEntry kDesktopLayout[] = {
    Area(kSidebarHost),
    Hint(kSettingsHint),
    Section(kSettingsSectionVideo),
    Setting(SettingField::Fps, kFpsLabel),
    Setting(SettingField::Bitrate, kBitrateLabel),
    Setting(SettingField::Quality, kQualityLabel),
    Section(kSettingsSectionSecurity),
    Setting(SettingField::Passcode, kPasscodeLabel),
    Hint(kPasscodeHint),
    Setting(SettingField::AllowInput, kAllowControlLabel),
    Section(kSettingsSectionSession),
    Setting(SettingField::ShareAudio, kShareAudioLabel),
    Setting(SettingField::TransferFolder, kTransferFolderLabel),
    Setting(SettingField::AutoShare, kAutoShareLabel),
    Setting(SettingField::Permissions, ""),

    Area(kSidebarClient),
    Setting(SettingField::PlayAudio, kPlayAudioLabel),

    Area(kSettingsGeneralArea),
    Section(kSettingsSectionConnection),
    Setting(SettingField::Port, kUdpPortLabel),
    Section(kSettingsSectionSession),
    Setting(SettingField::ClipboardSync, kClipboardSyncLabel),
    Setting(SettingField::KeepAwake, kKeepAwakeLabel),
    Section(kSettingsSectionLaunch),
    Setting(SettingField::Autostart, kAutostartLabel),
    Setting(SettingField::CloseToTray, kCloseToTrayLabel),
};

}

std::span<const SettingsEntry> DesktopSettingsLayout() {
    return kDesktopLayout;
}

bool* SettingFlag(UiSettings& settings, SettingField field) {
    switch (field) {
        case SettingField::AllowInput: return &settings.allowInput;
        case SettingField::ShareAudio: return &settings.shareAudio;
        case SettingField::AutoShare: return &settings.autoShare;
        case SettingField::PlayAudio: return &settings.playAudio;
        case SettingField::ClipboardSync: return &settings.clipboardSync;
        case SettingField::KeepAwake: return &settings.keepAwake;
        case SettingField::Autostart: return &settings.autostart;
        case SettingField::CloseToTray: return &settings.startHidden;
        case SettingField::None:
        case SettingField::Fps:
        case SettingField::Bitrate:
        case SettingField::Quality:
        case SettingField::Passcode:
        case SettingField::TransferFolder:
        case SettingField::Permissions:
        case SettingField::Port:
        case SettingField::Count: break;
    }
    return nullptr;
}

}
