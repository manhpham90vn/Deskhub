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

inline constexpr size_t kShortFingerprintChars = 12;
std::string ShortFingerprint(const Fingerprint& fp);

enum class TrustVerdict : uint8_t {
    Unknown = 0,
    Trusted = 1,
};

struct TrustedHost {
    Fingerprint fingerprint{};
    std::string label{};
    std::string endpoint{};
    int64_t firstSeenUnix = 0;
    int64_t lastSeenUnix = 0;

    bool operator==(const TrustedHost&) const = default;
};

inline constexpr size_t kMaxTrustedHosts = 256;
inline constexpr size_t kMaxTrustLabelBytes = 64;
inline constexpr size_t kMaxTrustEndpointBytes = 64;

class TrustStore {
public:
    TrustVerdict Check(const Fingerprint& fp) const;
    void Remember(const Fingerprint& fp, std::string_view label, std::string_view endpoint,
        int64_t nowUnix);
    bool Touch(const Fingerprint& fp, std::string_view endpoint, int64_t nowUnix);
    void Insert(const TrustedHost& host);
    bool SetLabel(const Fingerprint& fp, std::string_view label);
    bool Forget(const Fingerprint& fp);
    void Clear();

    std::optional<TrustedHost> Find(const Fingerprint& fp) const;
    std::optional<TrustedHost> FindByEndpoint(std::string_view endpoint) const;
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
std::optional<TrustStore> ParseTrustStoreStrict(std::string_view text);
std::string SerializeTrustStore(const TrustStore& store);

}
