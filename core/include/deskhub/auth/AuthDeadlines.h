#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace deskhub {

class AuthDeadlines {
public:
    explicit AuthDeadlines(size_t capacity);

    bool Admit(uint64_t key, uint64_t deadlineUs);
    void Hasten(uint64_t key, uint64_t deadlineUs);
    void Forget(uint64_t key);
    bool Due(uint64_t key, uint64_t nowUs) const;
    std::vector<uint64_t> TakeDue(uint64_t nowUs);
    void Clear();

private:
    size_t capacity_;
    std::map<uint64_t, uint64_t> deadlines_{};
};

}
