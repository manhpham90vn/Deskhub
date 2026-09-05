#include "deskhubp/ffi/DiscoveryFfi.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "deskhub/net/Ipv4.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/DeviceRows.h"
#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/client/DeviceStatusPoller.h"
#include "deskhubp/client/LanScanner.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Autostart.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

namespace ui = deskhub::ui;

constexpr const char* kRecentDevicesFile = "recent-devices.txt";
constexpr auto kScanSettleStep = std::chrono::milliseconds(20);

std::mutex g_mutex;
std::atomic<bool> g_restartPending{false};
std::atomic<uint32_t> g_scanEpoch{0};

deskhubp::LanScanner& Scanner() {
    static deskhubp::LanScanner scanner;
    return scanner;
}

std::vector<deskhubp::ScanHit> g_hits;
std::vector<std::string> g_hitsThisRound;
deskhubp::ScanProgress g_progress;
bool g_scanning = false;
bool g_scanFinished = false;

deskhubp::DeviceStatusPoller g_poller;
std::map<uint32_t, deskhubp::DeviceStatus>& Status() {
    static std::map<uint32_t, deskhubp::DeviceStatus> statusByIp;
    return statusByIp;
}
bool g_pollerStarted = false;

std::vector<ui::RecentDevice> g_recent;
bool g_recentLoaded = false;

std::vector<ui::RecentDevice>& Recent() {
    if (!g_recentLoaded) {
        g_recent = ui::ParseRecentDevices(deskhubp::ReadAppDataFile(kRecentDevicesFile));
        g_recentLoaded = true;
    }
    return g_recent;
}

void SaveRecent() {
    deskhubp::WriteAppDataFile(kRecentDevicesFile, ui::SerializeRecentDevices(g_recent));
}

std::string LocalTimeText(int64_t unixTime) {
    if (unixTime <= 0) return {};
    const std::time_t stamp = std::time_t(unixTime);
    std::tm parts{};
#if defined(_WIN32)
    if (localtime_s(&parts, &stamp) != 0) return {};
#else
    if (!localtime_r(&stamp, &parts)) return {};
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &parts) == 0) return {};
    return std::string(buf);
}

int FillText(char* out, int capacity, const std::string& text) {
    if (!out || capacity <= 0) return int(text.size());
    deskhubp::CopyToBuf(out, size_t(capacity), text);
    return int(std::strlen(out));
}

uint32_t IpOf(const std::string& addr) {
    NetAddr parsed{};
    if (!ParseNetAddr(addr, parsed)) return 0;
    return parsed.ip;
}

std::vector<std::string> PolledAddressesLocked() {
    std::vector<std::string> list;
    std::vector<uint32_t> seen;
    const auto add = [&list, &seen](const std::string& addr) {
        const uint32_t ip = IpOf(addr);
        if (!ip || std::find(seen.begin(), seen.end(), ip) != seen.end()) return;
        seen.push_back(ip);
        list.push_back(addr);
    };
    for (const ui::RecentDevice& device : Recent()) add(device.addr);
    for (const deskhubp::ScanHit& hit : g_hits) add(hit.addr);
    return list;
}

std::optional<deskhubp::DeviceStatus> StatusForLocked(const std::string& addr) {
    const uint32_t ip = IpOf(addr);
    if (!ip) return std::nullopt;
    const auto found = Status().find(ip);
    if (found == Status().end()) return std::nullopt;
    return found->second;
}

void RememberStatusLocked(const deskhubp::DeviceStatus& status) {
    const uint32_t ip = IpOf(status.addr);
    if (ip) Status()[ip] = status;
}

}

extern "C" {

bool dh_scan_start(uint16_t port) {
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_scanning) return false;
        g_hitsThisRound.clear();
        g_progress = deskhubp::ScanProgress{};
        g_scanning = true;
    }

    const bool started = Scanner().Start(
        port, [](const std::function<void()>& fn) { fn(); },
        [](const deskhubp::ScanHit& hit) {
            std::vector<std::string> polled;
            {
                std::lock_guard<std::mutex> lk(g_mutex);
                g_hitsThisRound.push_back(hit.addr);
                if (!StatusForLocked(hit.addr))
                    RememberStatusLocked(deskhubp::DeviceStatus{hit.addr, true, hit.rttMs});
                const auto same = [&hit](const deskhubp::ScanHit& known) {
                    return known.addr == hit.addr;
                };
                const auto known = std::find_if(g_hits.begin(), g_hits.end(), same);
                if (known != g_hits.end()) {
                    known->rttMs = hit.rttMs;
                    return;
                }
                g_hits.push_back(hit);
                polled = PolledAddressesLocked();
            }
            g_poller.SetAddresses(std::move(polled));
        },
        [](const deskhubp::ScanProgress& progress) {
            std::lock_guard<std::mutex> lk(g_mutex);
            g_progress = progress;
        },
        [](const deskhubp::ScanProgress& progress) {
            std::vector<std::string> polled;
            {
                std::lock_guard<std::mutex> lk(g_mutex);
                const auto gone = [](const deskhubp::ScanHit& hit) {
                    return std::find(g_hitsThisRound.begin(), g_hitsThisRound.end(), hit.addr) ==
                           g_hitsThisRound.end();
                };
                g_hits.erase(std::remove_if(g_hits.begin(), g_hits.end(), gone), g_hits.end());
                g_progress = progress;
                g_progress.found = g_hits.size();
                g_scanning = false;
                g_scanFinished = true;
                polled = PolledAddressesLocked();
            }
            g_poller.SetAddresses(std::move(polled));
        });

    if (!started) {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_scanning = false;
    }
    return started;
}

bool dh_scan_restart(uint16_t port) {
    dh_scan_cancel();
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_hits.clear();
        g_hitsThisRound.clear();
        g_progress = deskhubp::ScanProgress{};
        g_scanFinished = false;
    }

    if (!Scanner().Busy()) return dh_scan_start(port);
    if (g_restartPending.exchange(true)) return false;

    std::thread([port, epoch = g_scanEpoch.load()] {
        while (Scanner().Busy()) std::this_thread::sleep_for(kScanSettleStep);
        g_restartPending.store(false);
        if (g_scanEpoch.load() == epoch) dh_scan_start(port);
    }).detach();
    return true;
}

void dh_scan_cancel(void) {
    Scanner().Cancel();
    g_scanEpoch.fetch_add(1);
    std::lock_guard<std::mutex> lk(g_mutex);
    g_scanning = false;
}

uint32_t dh_scan_rescan_secs(void) {
    return deskhubp::kLanRescanSecs;
}

DHScanState dh_scan_state(void) {
    std::lock_guard<std::mutex> lk(g_mutex);
    DHScanState state{};
    state.probed = uint32_t(g_progress.probed);
    state.total = uint32_t(g_progress.total);
    state.found = uint32_t(g_hits.size());
    state.running = g_scanning;
    return state;
}

int dh_scan_hits(DHScanHit* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    const int count = int(g_hits.size()) < capacity ? int(g_hits.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const deskhubp::ScanHit& hit = g_hits[size_t(i)];
        const std::optional<deskhubp::DeviceStatus> status = StatusForLocked(hit.addr);
        deskhubp::CopyToBuf(out[i].addr, sizeof(out[i].addr), hit.addr);
        out[i].rttMs = status && status->online ? status->rttMs : hit.rttMs;
    }
    return count;
}

void dh_recent_touch(const char* address, const char* passcode) {
    if (!address || !*address) return;
    std::vector<std::string> polled;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        ui::TouchRecentDevice(Recent(), address, int64_t(std::time(nullptr)),
            passcode ? passcode : "");
        SaveRecent();
        polled = PolledAddressesLocked();
    }
    g_poller.SetAddresses(std::move(polled));
}

void dh_recent_remove(const char* address) {
    if (!address || !*address) return;
    std::vector<std::string> polled;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        ui::RemoveRecentDevice(Recent(), address);
        SaveRecent();
        polled = PolledAddressesLocked();
    }
    g_poller.SetAddresses(std::move(polled));
}

int dh_recent_passcode(const char* address, char* out, int capacity) {
    if (!address || !out || capacity <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    return FillText(out, capacity, ui::PasscodeForDevice(Recent(), address));
}

void dh_status_stop(void) {
    g_poller.Stop();
    std::lock_guard<std::mutex> lk(g_mutex);
    g_pollerStarted = false;
}

void dh_status_refresh_now(void) {
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        Status().clear();
    }
    g_poller.RefreshNow();
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
    deskhubp::CopyToBuf(out.passcode, sizeof(out.passcode), loaded.passcode);
    return out;
}

void dh_settings_save(uint32_t fps, uint32_t bitrate_mbps, uint32_t max_dim, uint32_t port,
    bool allow_input, bool client_control, const char* passcode) {
    ui::UiSettings out = deskhubp::LoadUiSettings();
    out.fps = fps;
    out.bitrateMbps = bitrate_mbps;
    out.maxDim = max_dim;
    out.port = port;
    out.allowInput = allow_input;
    out.clientControl = client_control;
    if (passcode && deskhub::IsValidPasscode(passcode)) out.passcode = passcode;
    deskhubp::SaveUiSettings(out);
}

int dh_recent_rows(DHRecentRow* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    const std::vector<ui::RecentDevice>& devices = Recent();
    const int count = int(devices.size()) < capacity ? int(devices.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const ui::RecentDevice& device = devices[size_t(i)];
        const std::optional<deskhubp::DeviceStatus> found = StatusForLocked(device.addr);
        const bool known = found.has_value();
        const bool online = known && found->online;

        deskhubp::CopyToBuf(out[i].addr, sizeof(out[i].addr), device.addr);
        deskhubp::CopyToBuf(out[i].passcode, sizeof(out[i].passcode), device.passcode);
        deskhubp::CopyToBuf(out[i].status, sizeof(out[i].status),
            !known ? ui::kStatusChecking : (online ? ui::kStatusOnline : ui::kStatusOffline));
        deskhubp::CopyToBuf(out[i].ping, sizeof(out[i].ping),
            online ? ui::PingMs(found->rttMs) : std::string("-"));
        deskhubp::CopyToBuf(out[i].lastConnected, sizeof(out[i].lastConnected),
            LocalTimeText(device.lastConnectedUnix));
        out[i].online = online;
    }
    return count;
}

int dh_device_rows(DHDeviceRow* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    std::vector<std::string> scanned;
    scanned.reserve(g_hits.size());
    for (const deskhubp::ScanHit& hit : g_hits) scanned.push_back(hit.addr);

    const std::vector<ui::DeviceRow> rows = ui::BuildDeviceRows(scanned, Recent());
    const int count = int(rows.size()) < capacity ? int(rows.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const ui::DeviceRow& row = rows[size_t(i)];
        const std::optional<deskhubp::DeviceStatus> found = StatusForLocked(row.addr);
        const bool known = found.has_value();
        const bool online = known && found->online;

        deskhubp::CopyToBuf(out[i].addr, sizeof(out[i].addr), row.addr);
        deskhubp::CopyToBuf(out[i].passcode, sizeof(out[i].passcode),
            ui::PasscodeForDevice(Recent(), row.addr));
        deskhubp::CopyToBuf(out[i].origin, sizeof(out[i].origin),
            ui::DeviceOriginLabel(row.origin));
        deskhubp::CopyToBuf(out[i].status, sizeof(out[i].status),
            !known ? ui::kStatusChecking : (online ? ui::kStatusOnline : ui::kStatusOffline));
        deskhubp::CopyToBuf(out[i].ping, sizeof(out[i].ping),
            online ? ui::PingMs(found->rttMs) : std::string("-"));
        const std::string last = LocalTimeText(row.lastConnectedUnix);
        deskhubp::CopyToBuf(out[i].lastConnected, sizeof(out[i].lastConnected),
            last.empty() ? std::string("-") : last);
        out[i].known = known;
        out[i].online = online;
    }
    return count;
}

void dh_status_watch_recent(void) {
    std::vector<std::string> list;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        list = PolledAddressesLocked();
    }
    g_poller.SetAddresses(std::move(list));

    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_pollerStarted) return;
    g_pollerStarted = true;
    g_poller.Start([](const deskhubp::DeviceStatus& status) {
        std::lock_guard<std::mutex> lk(g_mutex);
        RememberStatusLocked(status);
    });
}

int dh_ping_text(uint32_t rttMs, char* out, int capacity) {
    return FillText(out, capacity, ui::PingMs(rttMs));
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

int dh_sharing_status(uint16_t port, const char* passcode, bool allow_input, bool screen,
    bool terminal, bool files, char* out, int capacity) {
    std::string text = ui::ShareSummaryLine(screen, terminal, files, port);
    text += "\n" + ui::PasscodeNote(passcode ? passcode : "");
    if (screen && !allow_input) text += std::string("\n") + ui::kViewOnlyNote;
    return FillText(out, capacity, text);
}

int dh_scan_status_text(uint16_t port, char* out, int capacity) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_scanning) {
        const std::string busy = ui::ScanningStatus(g_progress.probed, g_progress.total, port);
        return FillText(out, capacity, busy);
    }
    if (!g_scanFinished) return FillText(out, capacity, ui::kLanDevicesEmpty);
    return FillText(out, capacity,
        ui::LanDevicesNote(g_hits.size(), g_progress.total, deskhubp::kLanRescanSecs));
}

int dh_recent_note(char* out, int capacity) {
    std::lock_guard<std::mutex> lk(g_mutex);
    return FillText(out, capacity,
        ui::RecentDevicesNote(Recent().size(), deskhubp::kDeviceStatusRoundSecs));
}

int dh_paired_devices(DHPairedDevice* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const std::vector<deskhub::PairedDevice> devices = deskhubp::LoadPairedDevices().Devices();
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

bool dh_paired_forget(const char* fingerprint) {
    if (!fingerprint) return false;
    const std::optional<deskhub::Fingerprint> fp = deskhub::ParseFingerprint(fingerprint);
    return fp && deskhubp::ForgetPairedDevice(*fp);
}

void dh_paired_forget_all(void) {
    deskhubp::ForgetAllPairedDevices();
}

bool dh_allow_pairing(void) {
    return deskhubp::LoadUiSettings().allowNewPairings;
}

void dh_set_allow_pairing(bool allow) {
    deskhub::ui::UiSettings settings = deskhubp::LoadUiSettings();
    settings.allowNewPairings = allow;
    deskhubp::SaveUiSettings(settings);
}

int dh_own_fingerprint(char* out, int capacity) {
    const deskhubp::HostIdentity identity =
        deskhubp::LoadOrCreateHostIdentity(deskhubp::SessionDeviceName());
    return FillText(out, capacity,
        identity.Valid() ? deskhub::FormatFingerprint(identity.fingerprint) : std::string());
}

int dh_format_address(uint64_t addr_packed, char* out, int capacity) {
    return FillText(out, capacity, NetAddr::Unpack(addr_packed).ToString());
}
}
