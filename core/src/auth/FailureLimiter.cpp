#include "deskhub/auth/FailureLimiter.h"

namespace deskhub {

bool AuthFailureLimiter::Allow(const Fingerprint& key, uint32_t ip, uint64_t nowUs) const {
    for (const Entry& entry : entries_)
        if (entry.occupied && entry.ip == ip && entry.key == key)
            return nowUs >= entry.blockedUntilUs;
    return true;
}

void AuthFailureLimiter::RecordFailure(const Fingerprint& key, uint32_t ip, uint64_t nowUs) {
    Entry* selected = nullptr;
    for (Entry& entry : entries_) {
        if (entry.occupied && entry.ip == ip && entry.key == key) {
            selected = &entry;
            break;
        }
        if (!entry.occupied) {
            if (selected == nullptr || selected->occupied) selected = &entry;
            continue;
        }
        if (selected == nullptr ||
            (selected->occupied && entry.lastFailureUs < selected->lastFailureUs))
            selected = &entry;
    }
    if (selected == nullptr) return;
    if (!selected->occupied || selected->ip != ip || selected->key != key ||
        nowUs < selected->lastFailureUs ||
        nowUs - selected->lastFailureUs > kAuthFailureWindowUs) {
        *selected = Entry{};
        selected->key = key;
        selected->ip = ip;
        selected->occupied = true;
    }
    selected->lastFailureUs = nowUs;
    if (++selected->failures >= kAuthFailureLimit) {
        selected->failures = 0;
        selected->blockedUntilUs = nowUs + kAuthFailureCooldownUs;
    }
}

void AuthFailureLimiter::RecordSuccess(const Fingerprint& key, uint32_t ip) {
    for (Entry& entry : entries_)
        if (entry.occupied && entry.ip == ip && entry.key == key) {
            entry = Entry{};
            return;
        }
}

void AuthFailureLimiter::Clear() {
    entries_ = {};
}

}
