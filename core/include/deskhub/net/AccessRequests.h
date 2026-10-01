#pragma once

#include "deskhub/net/PublicKeyText.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kMaxAccessRequests = 16;
inline constexpr int64_t kAccessRequestTtlSeconds = 600;
inline constexpr size_t kMaxAccessRequestsFileBytes = 65536;
inline constexpr size_t kMaxAccessRequestAddressBytes = 64;

struct AccessRequest {
    PublicKeyText key{};
    std::string address{};
    int64_t requestedUnix = 0;
    bool denied = false;
};

bool SameKey(const PublicKeyText& a, const PublicKeyText& b);

class AccessRequests {
public:
    bool Add(AccessRequest request, int64_t nowUnix);
    bool Remove(const PublicKeyText& key);
    bool Deny(const PublicKeyText& key, int64_t nowUnix);
    bool IsDenied(const PublicKeyText& key) const;
    std::optional<AccessRequest> Find(const PublicKeyText& key) const;
    size_t Expire(int64_t nowUnix);
    const std::vector<AccessRequest>& Requests() const {
        return requests_;
    }

private:
    std::vector<AccessRequest> requests_{};
};

std::optional<AccessRequests> ParseAccessRequests(std::string_view text, int64_t nowUnix);
std::string SerializeAccessRequests(const AccessRequests& requests);

}
