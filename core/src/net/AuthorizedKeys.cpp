#include "deskhub/net/AuthorizedKeys.h"

#include <algorithm>

namespace deskhub {

namespace {

bool SameKey(const PublicKeyText& a, const PublicKeyText& b) {
    return a.algorithm == b.algorithm && a.blob == b.blob;
}

}

bool AuthorizedKeys::Add(PublicKeyText key) {
    if (FormatPublicKeyText(key).empty() || Contains(key) || keys_.size() >= kMaxAuthorizedKeys)
        return false;
    keys_.push_back(std::move(key));
    return true;
}

bool AuthorizedKeys::Remove(const PublicKeyText& key) {
    const auto at = std::remove_if(keys_.begin(), keys_.end(),
        [&](const PublicKeyText& stored) { return SameKey(stored, key); });
    if (at == keys_.end()) return false;
    keys_.erase(at, keys_.end());
    return true;
}

bool AuthorizedKeys::Contains(const PublicKeyText& key) const {
    return std::any_of(keys_.begin(), keys_.end(),
        [&](const PublicKeyText& stored) { return SameKey(stored, key); });
}

const std::vector<PublicKeyText>& AuthorizedKeys::Keys() const {
    return keys_;
}

std::optional<AuthorizedKeys> ParseAuthorizedKeys(std::string_view text) {
    if (text.size() > kMaxAuthorizedKeysFileBytes) return std::nullopt;
    AuthorizedKeys keys;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line.front() == '#') continue;
        const auto key = ParsePublicKeyText(line);
        if (!key || !keys.Add(*key)) return std::nullopt;
    }
    return keys;
}

std::string SerializeAuthorizedKeys(const AuthorizedKeys& keys) {
    std::string out;
    for (const PublicKeyText& key : keys.Keys()) {
        const std::string line = FormatPublicKeyText(key);
        if (line.empty()) return {};
        out += line;
        out += '\n';
    }
    return out;
}

}
