#pragma once
#include <atomic>
#include <cstddef>
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

class WindowPercentile {
public:
    static constexpr uint32_t kBucketUs = 512;
    static constexpr size_t kBuckets = 256;
    static constexpr uint32_t kCeilingUs = kBucketUs * kBuckets;

    struct Snapshot {
        uint32_t p50Us = 0;
        uint32_t p99Us = 0;
        uint32_t maxUs = 0;
        uint32_t count = 0;
    };

    void Add(uint32_t us) {
        const size_t slot = us / kBucketUs < kBuckets - 1 ? us / kBucketUs : kBuckets - 1;
        bucket_[slot].fetch_add(1, std::memory_order_relaxed);
        count_.fetch_add(1, std::memory_order_relaxed);
        uint32_t cur = max_.load(std::memory_order_relaxed);
        while (us > cur && !max_.compare_exchange_weak(cur, us, std::memory_order_relaxed)) {}
    }

    Snapshot TakeReset() {
        Snapshot out;
        out.maxUs = max_.exchange(0, std::memory_order_relaxed);
        out.count = count_.exchange(0, std::memory_order_relaxed);
        if (!out.count) {
            for (std::atomic<uint32_t>& b : bucket_) b.store(0, std::memory_order_relaxed);
            return out;
        }
        const uint32_t rank50 = (out.count + 1) / 2;
        const uint32_t rank99 = uint32_t((uint64_t(out.count) * 99 + 99) / 100);
        uint32_t seen = 0;
        for (size_t slot = 0; slot < kBuckets; ++slot) {
            seen += bucket_[slot].exchange(0, std::memory_order_relaxed);
            const uint32_t edge = uint32_t(slot + 1) * kBucketUs;
            const uint32_t value =
                slot + 1 < kBuckets && edge < out.maxUs ? edge : out.maxUs;
            if (!out.p50Us && seen >= rank50) out.p50Us = value;
            if (!out.p99Us && seen >= rank99) out.p99Us = value;
        }
        return out;
    }

private:
    std::atomic<uint32_t> bucket_[kBuckets] = {};
    std::atomic<uint32_t> count_{0};
    std::atomic<uint32_t> max_{0};
};

class WindowCount {
public:
    void Add(uint32_t n = 1) {
        n_.fetch_add(n, std::memory_order_relaxed);
    }
    uint32_t TakeReset() {
        return n_.exchange(0, std::memory_order_relaxed);
    }
    uint32_t peek() const {
        return n_.load(std::memory_order_relaxed);
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
