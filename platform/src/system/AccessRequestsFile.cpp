#include "deskhubp/system/AccessRequestsFile.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/ConfigFileLock.h"

namespace deskhubp {

namespace {

std::mutex& StoreMutex() {
    static std::mutex mutex;
    return mutex;
}

std::atomic<uint64_t>& Generation() {
    static std::atomic<uint64_t> generation{0};
    return generation;
}

std::atomic<bool>& InvalidReported() {
    static std::atomic<bool> reported{false};
    return reported;
}

std::optional<deskhub::AccessRequests> InvalidConfig() {
    if (!InvalidReported().exchange(true, std::memory_order_acq_rel))
        LOGE("access_requests: the file cannot be read or parsed; it is replaced on the next request");
    return std::nullopt;
}

std::optional<deskhub::AccessRequests> LoadLocked() {
    const auto path = AppDataFilePath(kAccessRequestsFileName);
    if (path.empty()) return InvalidConfig();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::error_code error;
        if (std::filesystem::exists(path, error) || error) return InvalidConfig();
        InvalidReported().store(false, std::memory_order_release);
        return deskhub::AccessRequests{};
    }
    const auto size = file.tellg();
    if (size < 0 || size > std::streamoff(deskhub::kMaxAccessRequestsFileBytes) || !file.seekg(0))
        return InvalidConfig();
    std::string text(size_t(size), '\0');
    if (!text.empty() && !file.read(text.data(), size)) return InvalidConfig();
    auto requests = deskhub::ParseAccessRequests(text, NowUnixSeconds());
    if (!requests) return InvalidConfig();
    InvalidReported().store(false, std::memory_order_release);
    return requests;
}

bool SaveLocked(const deskhub::AccessRequests& requests) {
    if (!WriteAppDataFileAtomic(kAccessRequestsFileName, deskhub::SerializeAccessRequests(requests)))
        return false;
    Generation().fetch_add(1, std::memory_order_acq_rel);
    return true;
}

template <typename Change>
bool ChangeRequests(Change change) {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    const ConfigFileLock fileLock(kAccessRequestsFileName);
    if (!fileLock.Valid()) return false;
    deskhub::AccessRequests requests = LoadLocked().value_or(deskhub::AccessRequests{});
    if (!change(requests)) return false;
    return SaveLocked(requests);
}

std::optional<deskhub::Fingerprint> FingerprintOfKey(const deskhub::PublicKeyText& key) {
    return FingerprintOfPublicKey(PublicKeySpkiFromText(deskhub::FormatPublicKeyText(key)));
}

std::optional<deskhub::AccessRequest> FindPendingByFingerprint(
    const deskhub::AccessRequests& requests, const deskhub::Fingerprint& fingerprint) {
    for (const deskhub::AccessRequest& request : requests.Requests())
        if (!request.denied && FingerprintOfKey(request.key) == fingerprint) return request;
    return std::nullopt;
}

uint64_t FileStamp() {
    const auto path = AppDataFilePath(kAccessRequestsFileName);
    if (path.empty()) return 0;
    std::error_code error;
    const auto written = std::filesystem::last_write_time(path, error);
    if (error) return 0;
    const auto size = std::filesystem::file_size(path, error);
    if (error) return 0;
    return uint64_t(written.time_since_epoch().count()) ^ (uint64_t(size) << 40);
}

}

std::optional<deskhub::AccessRequests> LoadAccessRequests() {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    return LoadLocked();
}

bool RememberAccessRequest(std::span<const uint8_t> publicKeySpki, std::string_view clientName,
    std::string_view address) {
    auto key = deskhub::ParsePublicKeyText(PublicKeyTextFromSpki(publicKeySpki));
    if (!key) return false;
    key->label = std::string(clientName);
    if (deskhub::FormatPublicKeyText(*key).empty()) key->label.clear();
    const int64_t now = NowUnixSeconds();
    return ChangeRequests([&](deskhub::AccessRequests& requests) {
        requests.Expire(now);
        return requests.Add(deskhub::AccessRequest{*key, std::string(address), now}, now);
    });
}

bool ApproveAccessRequest(const deskhub::Fingerprint& fingerprint) {
    std::optional<deskhub::AccessRequest> approved;
    const bool removed = ChangeRequests([&](deskhub::AccessRequests& requests) {
        approved = FindPendingByFingerprint(requests, fingerprint);
        if (!approved) return false;
        if (!RememberAuthorizedKey(deskhub::FormatPublicKeyText(approved->key)) &&
            !IsClientKeyAuthorized(PublicKeySpkiFromText(deskhub::FormatPublicKeyText(approved->key))))
            return false;
        return requests.Remove(approved->key);
    });
    return removed;
}

bool DenyAccessRequest(const deskhub::Fingerprint& fingerprint) {
    return ChangeRequests([&](deskhub::AccessRequests& requests) {
        const auto denied = FindPendingByFingerprint(requests, fingerprint);
        return denied && requests.Deny(denied->key, NowUnixSeconds());
    });
}

std::optional<std::vector<PendingClient>> ListAccessRequests() {
    const auto requests = LoadAccessRequests();
    if (!requests) return std::nullopt;
    std::vector<PendingClient> clients;
    for (const deskhub::AccessRequest& request : requests->Requests()) {
        if (request.denied) continue;
        const auto fingerprint = FingerprintOfKey(request.key);
        if (!fingerprint) return std::nullopt;
        clients.push_back(PendingClient{request.key.label, request.address, request.requestedUnix,
            *fingerprint, deskhub::FormatPublicKeyText(request.key)});
    }
    std::sort(clients.begin(), clients.end(), [](const PendingClient& a, const PendingClient& b) {
        return a.requestedUnix > b.requestedUnix;
    });
    return clients;
}

uint64_t AccessRequestsGeneration() {
    return Generation().load(std::memory_order_acquire) + FileStamp();
}

}
