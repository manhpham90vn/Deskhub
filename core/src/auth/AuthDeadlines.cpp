#include "deskhub/auth/AuthDeadlines.h"

#include <algorithm>

namespace deskhub {

AuthDeadlines::AuthDeadlines(size_t capacity) : capacity_(capacity) {
}

bool AuthDeadlines::Admit(uint64_t key, uint64_t deadlineUs) {
    if (deadlines_.contains(key)) return true;
    if (deadlines_.size() >= capacity_) return false;
    deadlines_.emplace(key, deadlineUs);
    return true;
}

void AuthDeadlines::Hasten(uint64_t key, uint64_t deadlineUs) {
    const auto [at, added] = deadlines_.try_emplace(key, deadlineUs);
    if (!added) at->second = std::min(at->second, deadlineUs);
}

void AuthDeadlines::Forget(uint64_t key) {
    deadlines_.erase(key);
}

bool AuthDeadlines::Due(uint64_t key, uint64_t nowUs) const {
    const auto at = deadlines_.find(key);
    return at != deadlines_.end() && nowUs >= at->second;
}

std::vector<uint64_t> AuthDeadlines::TakeDue(uint64_t nowUs) {
    std::vector<uint64_t> due;
    for (auto at = deadlines_.begin(); at != deadlines_.end();) {
        if (nowUs < at->second) {
            ++at;
            continue;
        }
        due.push_back(at->first);
        at = deadlines_.erase(at);
    }
    return due;
}

void AuthDeadlines::Clear() {
    deadlines_.clear();
}

}
