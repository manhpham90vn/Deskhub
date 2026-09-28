#pragma once

#include "deskhub/net/PublicKeyText.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kMaxAuthorizedKeys = 128;
inline constexpr size_t kMaxAuthorizedKeysFileBytes = 262144;

class AuthorizedKeys {
public:
    bool Add(PublicKeyText key);
    bool Remove(const PublicKeyText& key);
    bool Contains(const PublicKeyText& key) const;
    const std::vector<PublicKeyText>& Keys() const;

private:
    std::vector<PublicKeyText> keys_{};
};

std::optional<AuthorizedKeys> ParseAuthorizedKeys(std::string_view text);
std::string SerializeAuthorizedKeys(const AuthorizedKeys& keys);

}
