#include "deskhubp/ffi/DevicesFfi.h"

#include <ctime>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "deskhub/net/TrustStore.h"
#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/system/RecentDevicesFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

namespace ui = deskhub::ui;

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

}

extern "C" {

int dh_device_rows(DHDeviceRow* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const std::vector<ui::RecentDevice> rows = deskhubp::LoadRecentDevices();
    const int count = int(rows.size()) < capacity ? int(rows.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const ui::RecentDevice& row = rows[size_t(i)];
        deskhubp::CopyToBuf(out[i].addr, sizeof(out[i].addr), row.addr);
        deskhubp::CopyToBuf(out[i].name, sizeof(out[i].name), row.name);
        const std::string last = LocalTimeText(row.lastConnectedUnix);
        deskhubp::CopyToBuf(out[i].lastConnected, sizeof(out[i].lastConnected),
            last.empty() ? std::string("-") : last);
    }
    return count;
}

int dh_paired_devices(DHPairedDevice* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const auto clients = deskhubp::ListAuthorizedClients();
    if (!clients) return 0;
    const int count = int(clients->size()) < capacity ? int(clients->size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const deskhubp::AuthorizedClient& client = (*clients)[size_t(i)];
        FillText(out[i].name, int(sizeof(out[i].name)), client.label);
        FillText(out[i].shortKey, int(sizeof(out[i].shortKey)),
            deskhub::ShortFingerprint(client.fingerprint));
        FillText(out[i].fingerprint, int(sizeof(out[i].fingerprint)),
            deskhub::FormatFingerprint(client.fingerprint));
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
    return fp && deskhubp::ForgetAuthorizedClient(*fp);
}

void dh_paired_forget_all(void) {
    deskhubp::ClearAuthorizedKeys();
}

int dh_host_fingerprint(char* out, int capacity) {
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    return FillText(out, capacity,
        identity.Valid() ? deskhub::FormatFingerprint(identity.fingerprint) : std::string());
}

int dh_host_public_key(char* out, int capacity) {
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    if (!identity.Valid()) return FillText(out, capacity, std::string());
    return FillText(out, capacity,
        deskhubp::IdentityPublicKeyLine(identity,
            ui::TruncateDeviceName(deskhubp::SessionDeviceName())));
}

int dh_access_requests(DHAccessRequest* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const auto requests = deskhubp::ListAccessRequests();
    if (!requests) return 0;
    const int count = int(requests->size()) < capacity ? int(requests->size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const deskhubp::PendingClient& client = (*requests)[size_t(i)];
        FillText(out[i].name, int(sizeof(out[i].name)), client.label);
        FillText(out[i].address, int(sizeof(out[i].address)), client.address);
        FillText(out[i].shortKey, int(sizeof(out[i].shortKey)),
            deskhub::ShortFingerprint(client.fingerprint));
        FillText(out[i].fingerprint, int(sizeof(out[i].fingerprint)),
            deskhub::FormatFingerprint(client.fingerprint));
        const std::string when = LocalTimeText(client.requestedUnix);
        FillText(out[i].requestedAt, int(sizeof(out[i].requestedAt)),
            when.empty() ? std::string("-") : when);
    }
    return count;
}

bool dh_access_approve(const char* fingerprint) {
    if (!fingerprint) return false;
    const std::optional<deskhub::Fingerprint> fp = deskhub::ParseFingerprint(fingerprint);
    return fp && deskhubp::ApproveAccessRequest(*fp);
}

bool dh_access_deny(const char* fingerprint) {
    if (!fingerprint) return false;
    const std::optional<deskhub::Fingerprint> fp = deskhub::ParseFingerprint(fingerprint);
    return fp && deskhubp::DenyAccessRequest(*fp);
}

uint64_t dh_access_requests_generation(void) {
    return deskhubp::AccessRequestsGeneration();
}
}
