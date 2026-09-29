#include "deskhubp/ffi/DiscoveryFfi.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstring>
#include <mutex>
#include <optional>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "deskhub/net/Ipv4.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/DeviceRows.h"
#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/SettingsLayout.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/Autostart.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

namespace ui = deskhub::ui;

constexpr const char* kRecentDevicesFile = "recent-devices.txt";
std::mutex g_mutex;

std::vector<ui::RecentDevice> g_recent;
bool g_recentLoaded = false;
bool g_recentNeedsMigration = false;

std::vector<ui::RecentDevice>& Recent() {
    if (!g_recentLoaded) {
        const std::string text = deskhubp::ReadAppDataFile(kRecentDevicesFile);
        g_recent = ui::ParseRecentDevices(text);
        g_recentNeedsMigration = !text.empty() && text != ui::SerializeRecentDevices(g_recent);
        g_recentLoaded = true;
    }
    if (g_recentNeedsMigration)
        g_recentNeedsMigration = !deskhubp::WriteAppDataFileAtomic(kRecentDevicesFile,
            ui::SerializeRecentDevices(g_recent));
    return g_recent;
}

void SaveRecent() {
    g_recentNeedsMigration = !deskhubp::WriteAppDataFileAtomic(kRecentDevicesFile,
        ui::SerializeRecentDevices(g_recent));
}

std::string LocalTimeText(int64_t unixTime) {
    if (unixTime <= 0) return {};
    const std::time_t stamp = std::time_t(unixTime);
    std::tm parts{};
#ifdef _WIN32
    if (localtime_s(&parts, &stamp) != 0) return {};
#else
    if (!localtime_r(&stamp, &parts)) return {};
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &parts) == 0) return {};
    return std::string(buf);
}

using deskhubp::FillText;

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

}

extern "C" {

void dh_recent_touch(const char* address) {
    if (!address || !*address) return;
    std::lock_guard<std::mutex> lk(g_mutex);
    ui::TouchRecentDevice(Recent(), address, int64_t(std::time(nullptr)));
    SaveRecent();
}

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

int dh_device_rows(DHDeviceRow* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    const std::vector<ui::DeviceRow> rows = ui::BuildDeviceRows({}, Recent());
    const int count = int(rows.size()) < capacity ? int(rows.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const ui::DeviceRow& row = rows[size_t(i)];
        deskhubp::CopyToBuf(out[i].addr, sizeof(out[i].addr), row.addr);
        deskhubp::CopyToBuf(out[i].origin, sizeof(out[i].origin),
            ui::DeviceOriginLabel(row.origin));
        deskhubp::CopyToBuf(out[i].status, sizeof(out[i].status), "-");
        deskhubp::CopyToBuf(out[i].ping, sizeof(out[i].ping), "-");
        const std::string last = LocalTimeText(row.lastConnectedUnix);
        deskhubp::CopyToBuf(out[i].lastConnected, sizeof(out[i].lastConnected),
            last.empty() ? std::string("-") : last);
        out[i].known = false;
        out[i].online = false;
    }
    return count;
}

bool dh_same_device_addr(const char* a, const char* b) {
    return a && b && ui::SameDeviceAddr(a, b);
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

int dh_paired_devices(DHPairedDevice* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const auto loaded = deskhubp::LoadEffectiveAuthorizedDevices();
    if (!loaded) return 0;
    const std::vector<deskhub::PairedDevice>& devices = loaded->Devices();
    const int count = int(devices.size()) < capacity ? int(devices.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const deskhub::PairedDevice& device = devices[size_t(i)];
        FillText(out[i].name, int(sizeof(out[i].name)), device.name);
        FillText(out[i].shortKey, int(sizeof(out[i].shortKey)),
            deskhub::ShortFingerprint(device.fingerprint));
        FillText(out[i].fingerprint, int(sizeof(out[i].fingerprint)),
            deskhub::FormatFingerprint(device.fingerprint));
        out[i].pairedUnix = device.pairedUnix;
        out[i].lastSeenUnix = device.lastSeenUnix;
    }
    return count;
}

bool dh_paired_add_public_key(const char* public_key) {
    if (!public_key) return false;
    return deskhubp::RememberAuthorizedKey(public_key);
}

bool dh_paired_forget(const char* fingerprint) {
    if (!fingerprint) return false;
    const std::optional<deskhub::Fingerprint> fp = deskhub::ParseFingerprint(fingerprint);
    return fp && deskhubp::ForgetEffectiveAuthorizedDevice(*fp);
}

void dh_paired_forget_all(void) {
    deskhubp::ClearAuthorizedKeys();
}

int dh_host_fingerprint(char* out, int capacity) {
    const deskhubp::HostIdentity identity =
        deskhubp::LoadOrCreateHostIdentity(deskhubp::SessionDeviceName());
    return FillText(out, capacity,
        identity.Valid() ? deskhub::FormatFingerprint(identity.fingerprint) : std::string());
}
}
