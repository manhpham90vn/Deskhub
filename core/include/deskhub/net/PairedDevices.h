#pragma once
#include "deskhub/net/TrustStore.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kMaxPairedDevices = 128;
inline constexpr size_t kMaxPairedNameBytes = 64;

struct PairedDevice {
    Fingerprint fingerprint{};
    std::string name{};
    int64_t pairedUnix = 0;
    int64_t lastSeenUnix = 0;

    bool operator==(const PairedDevice&) const = default;
};

enum class PairVerdict : uint8_t {
    Unknown = 0,
    Paired = 1,
};

class PairedDevices {
public:
    PairVerdict Check(const Fingerprint& fp) const;
    void Remember(const Fingerprint& fp, std::string_view name, int64_t nowUnix);
    void Insert(const PairedDevice& device);
    bool Touch(const Fingerprint& fp, std::string_view name, int64_t nowUnix);
    bool Forget(const Fingerprint& fp);
    void Clear();

    std::optional<PairedDevice> Find(const Fingerprint& fp) const;
    const std::vector<PairedDevice>& Devices() const {
        return devices_;
    }
    size_t Size() const {
        return devices_.size();
    }

private:
    std::vector<PairedDevice> devices_{};
};

std::optional<PairedDevices> ParsePairedDevicesStrict(std::string_view text);
std::string SerializePairedDevices(const PairedDevices& devices);

inline constexpr size_t kShortFingerprintChars = 12;
std::string ShortFingerprint(const Fingerprint& fp);

}
