#include "deskhubp/system/PairingTokenFile.h"

#include <mutex>
#include <string>

#include "deskhub/net/PairingInvite.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/ConfigFileLock.h"
#include "deskhubp/system/Random.h"

namespace deskhubp {

namespace {

std::mutex& StoreMutex() {
    static std::mutex mutex;
    return mutex;
}

deskhub::PairingTokens LoadLocked(int64_t nowUnix) {
    const std::string text = ReadAppDataFile(kPairingTokensFileName);
    const auto tokens = deskhub::ParsePairingTokens(text, nowUnix);
    if (!tokens) LOGW("pairing_tokens: the file cannot be parsed; starting from an empty one");
    return tokens.value_or(deskhub::PairingTokens{});
}

bool SaveLocked(const deskhub::PairingTokens& tokens) {
    if (tokens.Tokens().empty()) {
        RemoveAppDataFile(kPairingTokensFileName);
        return true;
    }
    return WriteAppDataFileAtomic(kPairingTokensFileName, deskhub::SerializePairingTokens(tokens));
}

template <typename Change>
bool ChangeTokens(Change change) {
    const std::lock_guard<std::mutex> lock(StoreMutex());
    const ConfigFileLock fileLock(kPairingTokensFileName);
    if (!fileLock.Valid()) return false;
    deskhub::PairingTokens tokens = LoadLocked(NowUnixSeconds());
    if (!change(tokens)) return false;
    return SaveLocked(tokens);
}

}

std::optional<deskhub::PairingToken> IssuePairingToken(int64_t ttlSeconds) {
    deskhub::PairingToken token{};
    if (ttlSeconds <= 0 || !RandomBytes(token.data(), token.size()) || deskhub::IsZero(token))
        return std::nullopt;
    const int64_t now = NowUnixSeconds();
    const bool issued = ChangeTokens([&](deskhub::PairingTokens& tokens) {
        return tokens.Issue(token, now + ttlSeconds, now);
    });
    if (!issued) return std::nullopt;
    return token;
}

bool RedeemPairingToken(std::span<const uint8_t> presented) {
    const int64_t now = NowUnixSeconds();
    return ChangeTokens(
        [&](deskhub::PairingTokens& tokens) { return tokens.Consume(presented, now); });
}

bool RevokePairingTokens() {
    return ChangeTokens([](deskhub::PairingTokens& tokens) {
        tokens.Clear();
        return true;
    });
}

}
