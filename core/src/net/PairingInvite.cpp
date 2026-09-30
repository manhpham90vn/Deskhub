#include "deskhub/net/PairingInvite.h"

#include "deskhub/net/Base64.h"
#include "deskhub/net/Ipv4.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr size_t kEndpointBytes = 6;
constexpr size_t kMaxRecordBytes = 1 + 1 + kMaxPairingEndpoints * kEndpointBytes +
                                   kFingerprintBytes + kPairingTokenBytes + 1 +
                                   kMaxPairingHostNameBytes;

std::string_view TrimSpaces(std::string_view text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

bool IsContinuationByte(uint8_t byte) {
    return (byte & 0xC0) == 0x80;
}

std::string CleanHostName(std::string_view name) {
    std::string out;
    for (char c : name) {
        const uint8_t byte = uint8_t(c);
        if (byte < 0x20 || byte == 0x7F) continue;
        out.push_back(c);
    }
    while (out.size() > kMaxPairingHostNameBytes) {
        out.pop_back();
        while (!out.empty() && IsContinuationByte(uint8_t(out.back()))) out.pop_back();
    }
    return std::string(TrimSpaces(out));
}

bool ValidHostName(std::string_view name) {
    if (name.size() > kMaxPairingHostNameBytes) return false;
    for (char c : name) {
        const uint8_t byte = uint8_t(c);
        if (byte < 0x20 || byte == 0x7F) return false;
    }
    return TrimSpaces(name).size() == name.size();
}

void PutU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(uint8_t(value >> 24));
    out.push_back(uint8_t(value >> 16));
    out.push_back(uint8_t(value >> 8));
    out.push_back(uint8_t(value));
}

void PutU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(uint8_t(value >> 8));
    out.push_back(uint8_t(value));
}

uint32_t ReadU32(const uint8_t* at) {
    return (uint32_t(at[0]) << 24) | (uint32_t(at[1]) << 16) | (uint32_t(at[2]) << 8) |
           uint32_t(at[3]);
}

uint16_t ReadU16(const uint8_t* at) {
    return uint16_t((uint16_t(at[0]) << 8) | uint16_t(at[1]));
}

bool ValidEndpoints(const std::vector<PairingEndpoint>& endpoints) {
    if (endpoints.empty() || endpoints.size() > kMaxPairingEndpoints) return false;
    for (const PairingEndpoint& endpoint : endpoints)
        if (endpoint.ip == 0 || endpoint.port == 0) return false;
    return true;
}

}

bool IsZero(const PairingToken& token) {
    return std::all_of(token.begin(), token.end(), [](uint8_t byte) { return byte == 0; });
}

bool IsPairingInvite(std::string_view text) {
    return TrimSpaces(text).starts_with(kPairingInvitePrefix);
}

std::optional<PairingEndpoint> MakePairingEndpoint(std::string_view ip, uint16_t port) {
    const std::optional<uint32_t> parsed = ParseIPv4(TrimSpaces(ip));
    if (!parsed || *parsed == 0 || port == 0) return std::nullopt;
    return PairingEndpoint{*parsed, port};
}

std::string FormatPairingEndpoint(const PairingEndpoint& endpoint) {
    return FormatIPv4(endpoint.ip) + ":" + std::to_string(endpoint.port);
}

std::string FormatPairingInvite(const PairingInvite& invite) {
    if (!ValidEndpoints(invite.endpoints) || IsZero(invite.hostKey) || IsZero(invite.token) ||
        !ValidHostName(invite.hostName))
        return {};
    std::vector<uint8_t> record;
    record.reserve(kMaxRecordBytes);
    record.push_back(kPairingInviteVersion);
    record.push_back(uint8_t(invite.endpoints.size()));
    for (const PairingEndpoint& endpoint : invite.endpoints) {
        PutU32(record, endpoint.ip);
        PutU16(record, endpoint.port);
    }
    record.insert(record.end(), invite.hostKey.bytes.begin(), invite.hostKey.bytes.end());
    record.insert(record.end(), invite.token.begin(), invite.token.end());
    record.push_back(uint8_t(invite.hostName.size()));
    record.insert(record.end(), invite.hostName.begin(), invite.hostName.end());
    return std::string(kPairingInvitePrefix) +
           EncodeBase64(record, Base64Alphabet::Url, Base64Padding::None);
}

std::optional<PairingInvite> ParsePairingInvite(std::string_view text) {
    std::string_view body = TrimSpaces(text);
    if (!body.starts_with(kPairingInvitePrefix) || body.size() > kMaxPairingInviteChars)
        return std::nullopt;
    body.remove_prefix(kPairingInvitePrefix.size());
    const std::optional<std::vector<uint8_t>> record = DecodeBase64(body, Base64Alphabet::Url);
    if (!record || record->size() < 2 || record->size() > kMaxRecordBytes) return std::nullopt;
    const uint8_t* at = record->data();
    const uint8_t* end = at + record->size();
    if (*at++ != kPairingInviteVersion) return std::nullopt;
    const size_t count = *at++;
    if (count == 0 || count > kMaxPairingEndpoints) return std::nullopt;
    const size_t fixedBytes = count * kEndpointBytes + kFingerprintBytes + kPairingTokenBytes + 1;
    if (size_t(end - at) < fixedBytes) return std::nullopt;

    PairingInvite invite;
    for (size_t i = 0; i < count; ++i) {
        invite.endpoints.push_back(PairingEndpoint{ReadU32(at), ReadU16(at + 4)});
        at += kEndpointBytes;
    }
    std::copy(at, at + kFingerprintBytes, invite.hostKey.bytes.begin());
    at += kFingerprintBytes;
    std::copy(at, at + kPairingTokenBytes, invite.token.begin());
    at += kPairingTokenBytes;
    const size_t nameLen = *at++;
    if (size_t(end - at) != nameLen) return std::nullopt;
    invite.hostName.assign(reinterpret_cast<const char*>(at), nameLen);
    if (!ValidEndpoints(invite.endpoints) || IsZero(invite.hostKey) || IsZero(invite.token) ||
        !ValidHostName(invite.hostName) || CleanHostName(invite.hostName) != invite.hostName)
        return std::nullopt;
    return invite;
}

}
