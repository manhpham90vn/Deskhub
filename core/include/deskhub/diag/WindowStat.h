#pragma once
#include <atomic>
#include <cstdint>

namespace deskhub::diag {

class WindowStat {
public:
    struct Snapshot {
        double avg = 0.0;
        uint32_t max = 0;
        uint32_t count = 0;
    };

    void Add(uint32_t v) {
        sum_.fetch_add(v, std::memory_order_relaxed);
        count_.fetch_add(1, std::memory_order_relaxed);
        uint32_t cur = max_.load(std::memory_order_relaxed);
        while (v > cur && !max_.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
    }

    Snapshot TakeReset() {
        const uint32_t s = sum_.exchange(0, std::memory_order_relaxed);
        const uint32_t m = max_.exchange(0, std::memory_order_relaxed);
        const uint32_t c = count_.exchange(0, std::memory_order_relaxed);
        return Snapshot{c ? double(s) / c : 0.0, m, c};
    }

private:
    std::atomic<uint32_t> sum_{0};
    std::atomic<uint32_t> max_{0};
    std::atomic<uint32_t> count_{0};
};

class WindowCount {
public:
    void Add(uint32_t n = 1) {
        n_.fetch_add(n, std::memory_order_relaxed);
    }
    uint32_t TakeReset() {
        return n_.exchange(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> n_{0};
};

class WindowMax {
public:
    void Add(uint32_t v) {
        uint32_t cur = v_.load(std::memory_order_relaxed);
        while (v > cur && !v_.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
    }
    uint32_t TakeReset() {
        return v_.exchange(0, std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> v_{0};
};

class RunningMin {
public:
    void Add(uint32_t v) {
        if (!v) return;
        uint32_t cur = v_.load(std::memory_order_relaxed);
        while ((cur == 0 || v < cur) &&
               !v_.compare_exchange_weak(cur, v, std::memory_order_relaxed)) {}
    }
    uint32_t value() const {
        return v_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<uint32_t> v_{0};
};

}
