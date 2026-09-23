#include "deskhub/session/host/ViewerTable.h"

namespace deskhub {

bool ViewerBudget::TryTake() {
    size_t current = taken_.load(std::memory_order_acquire);
    while (current < kMaxViewersPerHost) {
        if (taken_.compare_exchange_weak(current, current + 1, std::memory_order_acq_rel,
                std::memory_order_acquire))
            return true;
    }
    return false;
}

void ViewerBudget::Give(size_t count) {
    if (count) taken_.fetch_sub(count, std::memory_order_acq_rel);
}

ViewerTable::~ViewerTable() {
    if (budget_) budget_->Give(count_);
}

ViewerSlot* ViewerTable::Find(uint64_t addrPacked) {
    if (!addrPacked) return nullptr;
    for (ViewerSlot& s : slots_)
        if (s.active && s.addrPacked == addrPacked) return &s;
    return nullptr;
}

ViewerSlot* ViewerTable::FindByClient(uint32_t clientId) {
    for (ViewerSlot& s : slots_)
        if (s.active && s.clientId == clientId) return &s;
    return nullptr;
}

ViewerSlot* ViewerTable::Admit(uint32_t clientId, uint64_t addrPacked, uint64_t nowUs,
    std::string_view name) {
    if (!addrPacked) return nullptr;
    for (ViewerSlot& s : slots_) {
        if (s.active) continue;
        if (budget_ && !budget_->TryTake()) return nullptr;
        s = ViewerSlot{};
        s.active = true;
        s.clientId = clientId;
        s.addrPacked = addrPacked;
        s.joinOrder = nextJoinOrder_++;
        s.lastRecvUs = nowUs;
        s.name = std::string{name};
        ++count_;
        Publish();
        return &s;
    }
    return nullptr;
}

void ViewerTable::Rebind(ViewerSlot& slot, uint64_t addrPacked) {
    if (!slot.active || !addrPacked || slot.addrPacked == addrPacked) return;
    slot.addrPacked = addrPacked;
    Publish();
}

void ViewerTable::SetName(ViewerSlot& slot, std::string_view name) {
    if (!slot.active || slot.name == name) return;
    slot.name = std::string{name};
    Publish();
}

void ViewerTable::Drop(ViewerSlot& slot) {
    if (!slot.active) return;
    slot = ViewerSlot{};
    --count_;
    if (budget_) budget_->Give(1);
    Publish();
}

void ViewerTable::Clear() {
    for (ViewerSlot& s : slots_) s = ViewerSlot{};
    if (budget_) budget_->Give(count_);
    count_ = 0;
    Publish();
}

bool ViewerTable::HigherPriorityIsDriving(const ViewerSlot& slot, uint64_t nowUs) const {
    for (const ViewerSlot& s : slots_) {
        if (&s == &slot || s.joinOrder >= slot.joinOrder) continue;
        if (IsDriving(s, nowUs)) return true;
    }
    return false;
}

bool ViewerTable::anyStarted() const {
    for (const ViewerSlot& s : slots_)
        if (s.active && s.started) return true;
    return false;
}

void ViewerTable::SetWantsAudio(ViewerSlot& slot, bool on) {
    if (!slot.active || slot.wantsAudio == on) return;
    slot.wantsAudio = on;
    Publish();
}

size_t ViewerTable::SnapshotAudioAddrs(std::span<uint64_t> out) const {
    size_t n = 0;
    for (const std::atomic<uint64_t>& a : publishedAudio_) {
        if (n >= out.size()) break;
        const uint64_t packed = a.load(std::memory_order_acquire);
        if (packed) out[n++] = packed;
    }
    return n;
}

size_t ViewerTable::SnapshotAddrs(std::span<uint64_t> out) const {
    size_t n = 0;
    for (const std::atomic<uint64_t>& a : published_) {
        if (n >= out.size()) break;
        const uint64_t packed = a.load(std::memory_order_acquire);
        if (packed) out[n++] = packed;
    }
    return n;
}

size_t ViewerTable::SnapshotInfos(std::span<ViewerInfo> out) const {
    std::lock_guard<std::mutex> lk(publishMutex_);
    size_t n = 0;
    for (const ViewerInfo& v : publishedInfos_) {
        if (n >= out.size()) break;
        if (v.addrPacked) out[n++] = v;
    }
    return n;
}

InputReceiver::Stats ViewerTable::inputStats() const {
    InputReceiver::Stats total{};
    for (const ViewerSlot& s : slots_) {
        if (!s.active) continue;
        const InputReceiver::Stats& one = s.input.stats();
        total.packets += one.packets;
        total.applied += one.applied;
        total.duplicates += one.duplicates;
        total.lost += one.lost;
    }
    return total;
}

void ViewerTable::Publish() {
    std::lock_guard<std::mutex> lk(publishMutex_);
    size_t n = 0;
    for (const ViewerSlot& s : slots_) {
        if (!s.active) continue;
        publishedInfos_[n].addrPacked = s.addrPacked;
        publishedInfos_[n].name = s.name;
        published_[n].store(s.addrPacked, std::memory_order_release);
        publishedAudio_[n].store(s.wantsAudio ? s.addrPacked : 0, std::memory_order_release);
        ++n;
    }
    for (size_t i = n; i < kMaxViewersPerSource; ++i) {
        publishedInfos_[i] = ViewerInfo{};
        published_[i].store(0, std::memory_order_release);
        publishedAudio_[i].store(0, std::memory_order_release);
    }
    publishedCount_.store(n, std::memory_order_release);
}

Feedback WorstCaseFeedback(std::span<const ViewerSlot> slots) {
    Feedback worst{};
    bool any = false;
    for (const ViewerSlot& s : slots) {
        if (!s.active || !s.haveFeedback) continue;
        if (!any) {
            worst = s.feedback;
            any = true;
            continue;
        }
        if (s.feedback.lossPct > worst.lossPct) worst.lossPct = s.feedback.lossPct;
        if (s.feedback.rttMs > worst.rttMs) worst.rttMs = s.feedback.rttMs;
        if (s.feedback.recvBitrateKbps < worst.recvBitrateKbps)
            worst.recvBitrateKbps = s.feedback.recvBitrateKbps;
    }
    return worst;
}

}
