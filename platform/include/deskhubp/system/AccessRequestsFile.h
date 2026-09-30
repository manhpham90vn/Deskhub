#pragma once

#include "deskhub/net/AccessRequests.h"
#include "deskhub/net/TrustStore.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace deskhubp {

inline constexpr const char* kAccessRequestsFileName = "access_requests";

struct PendingClient {
    std::string label{};
    std::string address{};
    int64_t requestedUnix = 0;
    deskhub::Fingerprint fingerprint{};
    std::string publicKeyText{};
};

std::optional<deskhub::AccessRequests> LoadAccessRequests();
bool RememberAccessRequest(std::span<const uint8_t> publicKeySpki, std::string_view clientName,
    std::string_view address);
bool ApproveAccessRequest(const deskhub::Fingerprint& fingerprint);
bool DenyAccessRequest(const deskhub::Fingerprint& fingerprint);
std::optional<std::vector<PendingClient>> ListAccessRequests();
uint64_t AccessRequestsGeneration();

}
