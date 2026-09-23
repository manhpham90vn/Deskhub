#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kFingerprintBytes = 32;
inline constexpr size_t kFingerprintTextBytes = 43;
inline constexpr std::string_view kFingerprintPrefix = "SHA256:";

struct Fingerprint {
    std::array<uint8_t, kFingerprintBytes> bytes{};

    bool operator==(const Fingerprint&) const = default;
};

bool IsZero(const Fingerprint& fp);
std::string FormatFingerprint(const Fingerprint& fp);
std::optional<Fingerprint> ParseFingerprint(std::string_view text);

enum class TrustVerdict : uint8_t {
    Unknown = 0,
    Trusted = 1,
    Changed = 2,
};

struct TrustedHost {
    std::string endpoint{};
    std::string label{};
    Fingerprint fingerprint{};
    int64_t firstSeenUnix = 0;
    int64_t lastSeenUnix = 0;

    bool operator==(const TrustedHost&) const = default;
};

inline constexpr size_t kMaxTrustedHosts = 256;
inline constexpr size_t kMaxTrustLabelBytes = 64;

class TrustStore {
public:
    TrustVerdict Check(std::string_view endpoint, const Fingerprint& fp) const;
    void Remember(std::string_view endpoint, std::string_view label, const Fingerprint& fp,
        int64_t nowUnix);
    void Insert(const TrustedHost& host);
    bool Forget(std::string_view endpoint);
    void Clear();

    std::optional<TrustedHost> Find(std::string_view endpoint) const;
    const std::vector<TrustedHost>& Hosts() const {
        return hosts_;
    }
    size_t Size() const {
        return hosts_.size();
    }

private:
    std::vector<TrustedHost> hosts_{};
};

TrustStore ParseTrustStore(std::string_view text);
std::string SerializeTrustStore(const TrustStore& store);

}
