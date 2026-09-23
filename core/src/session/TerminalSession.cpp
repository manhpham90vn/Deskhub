#include "deskhub/session/TerminalSession.h"

#include <algorithm>

namespace deskhub {

void TerminalSessions::SetSharing(bool on) {
    sharing_ = on;
    if (!on) CloseAll();
}

TermOpenAck TerminalSessions::Refuse(TermReason reason) const {
    TermOpenAck ack;
    ack.reason = reason;
    return ack;
}

const TerminalRecord* TerminalSessions::Find(uint32_t termId) const {
    for (const TerminalRecord& r : records_)
        if (r.termId == termId) return &r;
    return nullptr;
}

TerminalRecord* TerminalSessions::Mutable(uint32_t termId) {
    for (TerminalRecord& r : records_)
        if (r.termId == termId) return &r;
    return nullptr;
}

TermOpenAck TerminalSessions::Open(const TerminalOpenRequest& request) {
    if (!sharing_) return Refuse(TermReason::NotShared);

    const TermSize size = ClampTermSize(request.message.size);

    if (request.message.resumeId != 0) {
        TerminalRecord* found = Mutable(request.message.resumeId);
        if (!found || found->state != TerminalState::Detached)
            return Refuse(TermReason::NoSuchSession);
        found->state = TerminalState::Live;
        found->size = size;
        found->detachedUs = 0;
        found->clientName = request.message.clientName;
        found->clientEndpoint = request.endpoint;
        found->clientFingerprint = request.fingerprint;
        TermOpenAck ack;
        ack.termId = found->termId;
        ack.resumed = true;
        return ack;
    }

    if (records_.size() >= kMaxTerminalSessions) return Refuse(TermReason::TooManySessions);

    TerminalRecord record;
    record.termId = nextId_++;
    if (nextId_ == 0) nextId_ = 1;
    record.size = size;
    record.clientName = request.message.clientName;
    record.clientEndpoint = request.endpoint;
    record.clientFingerprint = request.fingerprint;
    records_.push_back(std::move(record));

    TermOpenAck ack;
    ack.termId = records_.back().termId;
    return ack;
}

bool TerminalSessions::Resize(uint32_t termId, TermSize size) {
    TerminalRecord* found = Mutable(termId);
    if (!found) return false;
    found->size = ClampTermSize(size);
    return true;
}

bool TerminalSessions::Detach(uint32_t termId, uint64_t nowUs) {
    TerminalRecord* found = Mutable(termId);
    if (!found || found->state != TerminalState::Live) return false;
    found->state = TerminalState::Detached;
    found->detachedUs = nowUs;
    return true;
}

bool TerminalSessions::AttachLocal(uint32_t termId) {
    TerminalRecord* found = Mutable(termId);
    if (!found) return false;
    found->state = TerminalState::Local;
    found->detachedUs = 0;
    return true;
}

bool TerminalSessions::Close(uint32_t termId) {
    const auto at = std::remove_if(records_.begin(), records_.end(),
        [&](const TerminalRecord& r) { return r.termId == termId; });
    if (at == records_.end()) return false;
    records_.erase(at, records_.end());
    return true;
}

void TerminalSessions::CloseAll() {
    records_.clear();
}

TermSessionList TerminalSessions::List() const {
    TermSessionList out;
    for (const TerminalRecord& r : records_) {
        TermSessionEntry e;
        e.termId = r.termId;
        e.state = r.state;
        e.size = r.size;
        e.clientName = r.clientName;
        out.sessions.push_back(std::move(e));
    }
    return out;
}

std::string TerminalAuditLine(const TerminalRecord& record, std::string_view what) {
    std::string out = "terminal ";
    out += what;
    out += " id=" + std::to_string(record.termId);
    out += " from=" + (record.clientEndpoint.empty() ? std::string("?") : record.clientEndpoint);
    out += " name=" + (record.clientName.empty() ? std::string("?") : record.clientName);
    out += " key=";
    out += IsZero(record.clientFingerprint) ? std::string("none")
                                            : FormatFingerprint(record.clientFingerprint);
    return out;
}

}
