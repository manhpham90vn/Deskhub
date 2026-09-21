#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kMaxTerminalSessions = 8;
inline constexpr uint64_t kTerminalReattachGraceUs = 120'000'000;

struct TerminalRecord {
    uint32_t termId = 0;
    TerminalState state = TerminalState::Live;
    TermSize size{};
    std::string clientName{};
    std::string clientEndpoint{};
    Fingerprint clientFingerprint{};
    uint64_t openedUs = 0;
    uint64_t detachedUs = 0;
};

struct TerminalOpenRequest {
    TermOpen message{};
    std::string endpoint{};
    Fingerprint fingerprint{};
};

class TerminalSessions {
public:
    void SetSharing(bool on);
    void SetPasscode(std::string passcode);
    void SetConnectionAuthenticated(bool authenticated) {
        connectionAuthenticated_ = authenticated;
    }

    TermOpenAck Open(const TerminalOpenRequest& request, uint64_t nowUs);
    bool Resize(uint32_t termId, TermSize size);
    bool Detach(uint32_t termId, uint64_t nowUs);
    bool AttachLocal(uint32_t termId);
    bool Close(uint32_t termId);
    void CloseAll();
    TermSessionList List() const;

    bool Sharing() const {
        return sharing_;
    }
    const TerminalRecord* Find(uint32_t termId) const;
    const std::vector<TerminalRecord>& Records() const {
        return records_;
    }
    size_t Count() const {
        return records_.size();
    }
    size_t LiveCount() const;
    bool LockedOut(uint64_t nowUs) const;

private:
    TermOpenAck Refuse(TermReason reason) const;
    bool PasscodeAllows(std::string_view offered, uint64_t nowUs);
    TerminalRecord* Mutable(uint32_t termId);

    bool sharing_ = false;
    std::string passcode_{};
    bool connectionAuthenticated_ = false;
    uint32_t wrongPasscodes_ = 0;
    uint64_t lockUntilUs_ = 0;
    uint32_t nextId_ = 1;
    std::vector<TerminalRecord> records_{};
};

std::string TerminalAuditLine(const TerminalRecord& record, std::string_view what);

}
