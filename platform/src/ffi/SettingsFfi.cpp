#include "deskhubp/ffi/SettingsFfi.h"

#include <iterator>
#include <span>
#include <string>

#include "deskhub/net/Ipv4.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/SettingsLayout.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/system/Autostart.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

namespace ui = deskhub::ui;

struct KindPair {
    DHSettingsEntryKind ffi;
    ui::SettingsEntryKind core;
};

struct FieldPair {
    DHSettingField ffi;
    ui::SettingField core;
};

constexpr KindPair kKindPairs[] = {
    KindPair{DHSettingsEntryArea, ui::SettingsEntryKind::Area},
    KindPair{DHSettingsEntryHint, ui::SettingsEntryKind::Hint},
    KindPair{DHSettingsEntrySection, ui::SettingsEntryKind::Section},
    KindPair{DHSettingsEntrySetting, ui::SettingsEntryKind::Setting},
};

constexpr FieldPair kFieldPairs[] = {
    FieldPair{DHSettingNone, ui::SettingField::None},
    FieldPair{DHSettingFps, ui::SettingField::Fps},
    FieldPair{DHSettingBitrate, ui::SettingField::Bitrate},
    FieldPair{DHSettingQuality, ui::SettingField::Quality},
    FieldPair{DHSettingAllowInput, ui::SettingField::AllowInput},
    FieldPair{DHSettingShareAudio, ui::SettingField::ShareAudio},
    FieldPair{DHSettingTransferFolder, ui::SettingField::TransferFolder},
    FieldPair{DHSettingAutoShare, ui::SettingField::AutoShare},
    FieldPair{DHSettingPermissions, ui::SettingField::Permissions},
    FieldPair{DHSettingPlayAudio, ui::SettingField::PlayAudio},
    FieldPair{DHSettingPort, ui::SettingField::Port},
    FieldPair{DHSettingClipboardSync, ui::SettingField::ClipboardSync},
    FieldPair{DHSettingKeepAwake, ui::SettingField::KeepAwake},
    FieldPair{DHSettingAutostart, ui::SettingField::Autostart},
    FieldPair{DHSettingCloseToTray, ui::SettingField::CloseToTray},
    FieldPair{DHSettingDeviceName, ui::SettingField::DeviceName},
};

static_assert(std::size(kFieldPairs) == size_t(ui::SettingField::Count),
    "DHSettingField must name every ui::SettingField");

constexpr bool FfiSettingsLayoutMirrorsCore() {
    for (const KindPair& pair : kKindPairs)
        if (int(pair.ffi) != int(pair.core)) return false;
    for (const FieldPair& pair : kFieldPairs)
        if (int(pair.ffi) != int(pair.core)) return false;
    return true;
}

static_assert(FfiSettingsLayoutMirrorsCore(),
    "each DHSettingsEntryKind and DHSettingField must carry its core value");

using deskhubp::FillText;

}

extern "C" {

int dh_settings_layout(DHSettingsEntry* out, int capacity) {
    const std::span<const ui::SettingsEntry> layout = ui::DesktopSettingsLayout();
    const int count = int(layout.size());
    if (!out || capacity <= 0) return count;
    const int filled = capacity < count ? capacity : count;
    for (int i = 0; i < filled; ++i) {
        const ui::SettingsEntry& entry = layout[size_t(i)];
        out[i].kind = DHSettingsEntryKind(entry.kind);
        out[i].field = DHSettingField(entry.field);
        deskhubp::CopyToBuf(out[i].text, sizeof(out[i].text), entry.text);
    }
    return filled;
}

int dh_settings_area_bar_width(void) {
    return ui::kSettingsAreaBarWidth;
}

DHUiSettings dh_settings_load(void) {
    const ui::UiSettings loaded = deskhubp::LoadUiSettings();
    DHUiSettings out{};
    out.fps = loaded.fps;
    out.bitrateMbps = loaded.bitrateMbps;
    out.maxDim = loaded.maxDim;
    out.port = loaded.port;
    out.allowInput = loaded.allowInput;
    out.clientControl = loaded.clientControl;
    return out;
}

void dh_settings_save(uint32_t fps, uint32_t bitrate_mbps, uint32_t max_dim, uint32_t port,
    bool allow_input, bool client_control) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.fps = fps;
    out.bitrateMbps = bitrate_mbps;
    out.maxDim = max_dim;
    out.port = port;
    out.allowInput = allow_input;
    out.clientControl = client_control;
    deskhubp::SaveUiSettings(out);
}

uint16_t dh_default_port(void) {
    return deskhub::kDeskhubPort;
}

bool dh_client_control(void) {
    return deskhubp::LoadUiSettings().clientControl;
}

void dh_set_client_control(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.clientControl = on;
    deskhubp::SaveUiSettings(out);
}

int dh_device_name(char* out, int capacity) {
    return FillText(out, capacity, deskhubp::LoadUiSettings().deviceName);
}

void dh_set_device_name(const char* name) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.deviceName = ui::TruncateDeviceName(name ? name : "");
    deskhubp::SaveUiSettings(out);
}

int dh_bind_ip(char* out, int capacity) {
    return FillText(out, capacity, deskhubp::LoadUiSettings().bindIp);
}

void dh_set_bind_ip(const char* ip) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    const std::string requested = ip ? ip : "";
    out.bindIp = deskhub::ParseIPv4(requested) ? requested : "";
    deskhubp::SaveUiSettings(out);
}

bool dh_autostart_enabled(void) {
    return deskhubp::AutostartEnabled();
}

void dh_set_autostart(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.autostart = deskhubp::SetAutostartEnabled(on) ? on : deskhubp::AutostartEnabled();
    deskhubp::SaveUiSettings(out);
}

bool dh_auto_share(void) {
    return deskhubp::LoadUiSettings().autoShare;
}

void dh_set_auto_share(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.autoShare = on;
    deskhubp::SaveUiSettings(out);
}

bool dh_clipboard_sync(void) {
    return deskhubp::LoadUiSettings().clipboardSync;
}

void dh_set_clipboard_sync(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.clipboardSync = on;
    deskhubp::SaveUiSettings(out);
}

void dh_set_transfer_dir(const char* dir) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.transferDir = ui::TruncateSettingsPath(dir ? dir : "");
    deskhubp::SaveUiSettings(out);
}

bool dh_share_audio(void) {
    return deskhubp::LoadUiSettings().shareAudio;
}

void dh_set_share_audio(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.shareAudio = on;
    deskhubp::SaveUiSettings(out);
}

bool dh_play_audio(void) {
    return deskhubp::LoadUiSettings().playAudio;
}

void dh_set_play_audio(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.playAudio = on;
    deskhubp::SaveUiSettings(out);
}

bool dh_start_hidden(void) {
    return deskhubp::LoadUiSettings().startHidden;
}

void dh_set_start_hidden(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.startHidden = on;
    deskhubp::SaveUiSettings(out);
}

bool dh_keep_awake(void) {
    return deskhubp::LoadUiSettings().keepAwake;
}

void dh_set_keep_awake(bool on) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.keepAwake = on;
    deskhubp::SaveUiSettings(out);
}

int dh_version_line(char* out, int capacity) {
    return FillText(out, capacity, ui::VersionLine());
}

const char* dh_local_addresses(void) {
    static char buf[1024];
    std::string joined;
    for (const auto& a : ListLocalIPv4()) {
        if (!joined.empty()) joined += '\n';
        joined += a.ip + '\t' + a.name;
    }
    deskhubp::CopyToBuf(buf, sizeof(buf), joined);
    return buf;
}

int dh_idle_host_status(uint16_t port, char* out, int capacity) {
    return FillText(out, capacity, ui::UdpPortLine(port) + ".");
}

int dh_sharing_status(uint16_t port, bool allow_input, bool screen, bool terminal, bool files,
    char* out, int capacity) {
    std::string text = ui::ShareSummaryLine(screen, terminal, files, port);
    if (screen && !allow_input) text += std::string("\n") + ui::kViewOnlyNote;
    return FillText(out, capacity, text);
}
}
