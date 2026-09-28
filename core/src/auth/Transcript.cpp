#include "deskhub/auth/Transcript.h"

#include <algorithm>
#include <string_view>

namespace deskhub {

namespace {

constexpr std::string_view kDomain = "Deskhub/auth/signature";

void AppendField(std::vector<uint8_t>& out, std::span<const uint8_t> field) {
    const uint32_t size = uint32_t(field.size());
    out.push_back(uint8_t(size >> 24));
    out.push_back(uint8_t(size >> 16));
    out.push_back(uint8_t(size >> 8));
    out.push_back(uint8_t(size));
    out.insert(out.end(), field.begin(), field.end());
}

}

std::vector<uint8_t> AuthTranscript(AuthRole role,
    const AuthSessionId& sessionId,
    std::span<const uint8_t> clientPublicKey, const Fingerprint& hostFingerprint) {
    if ((role != AuthRole::Client && role != AuthRole::Host) ||
        std::all_of(sessionId.begin(), sessionId.end(), [](uint8_t byte) { return byte == 0; }) ||
        clientPublicKey.empty() ||
        clientPublicKey.size() > kMaxAuthBlobBytes || IsZero(hostFingerprint))
        return {};

    std::vector<uint8_t> out;
    out.reserve(5 * 4 + kDomain.size() + 2 + sessionId.size() + clientPublicKey.size() +
                hostFingerprint.bytes.size());
    AppendField(out, std::span<const uint8_t>(
                         reinterpret_cast<const uint8_t*>(kDomain.data()), kDomain.size()));
    const std::array<uint8_t, 2> metadata{kAuthVersion, uint8_t(role)};
    AppendField(out, metadata);
    AppendField(out, sessionId);
    AppendField(out, clientPublicKey);
    AppendField(out, hostFingerprint.bytes);
    return out;
}

}
