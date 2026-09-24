#include "deskhubp/ffi/TerminalFfi.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "deskhub/ui/ShellPicker.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/ffi/TermGridFill.h"
#include "deskhubp/client/TerminalViewer.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

using deskhubp::FillText;

void CopyInto(char* out, size_t capacity, const std::string& text) {
    const size_t take = std::min(capacity - 1, text.size());
    std::memcpy(out, text.data(), take);
    out[take] = '\0';
}

const deskhub::TermSessionEntry* EntryFor(const deskhub::TermSessionList& list, uint32_t termId) {
    for (const deskhub::TermSessionEntry& e : list.sessions)
        if (e.termId == termId) return &e;
    return nullptr;
}

std::vector<DHTermSessionInfo> InfosFor(const deskhub::TermSessionList& list) {
    std::vector<DHTermSessionInfo> infos;
    for (const deskhub::ui::ShellPickerRow& row : deskhub::ui::BuildShellPickerRows(list)) {
        const deskhub::TermSessionEntry* entry = EntryFor(list, row.termId);
        if (entry == nullptr) continue;
        DHTermSessionInfo info{};
        info.termId = entry->termId;
        info.state = int32_t(entry->state);
        info.cols = entry->size.cols;
        info.rows = entry->size.rows;
        info.resumable = row.resumable;
        info.closable = row.closable;
        CopyInto(info.name, sizeof(info.name), entry->clientName);
        CopyInto(info.line, sizeof(info.line), deskhub::ui::ShellPickerLine(row));
        infos.push_back(info);
    }
    return infos;
}

}

struct DHTermSession {
    std::mutex mutex{};
    std::vector<DHTermSessionInfo> sessions{};
    bool sessionsKnown = false;
    deskhubp::TerminalViewer viewer{};
    DHTermCallbacks callbacks{};
};

static DHTermSession* StartViewer(const char* address, const char* passcode, uint16_t cols,
    uint16_t rows, uint32_t resumeId, bool deferOpen, const DHTermCallbacks* callbacks) {
    if (address == nullptr) return nullptr;
    NetAddr server;
    if (!ParseNetAddr(address, server)) return nullptr;

    auto* session = new DHTermSession;
    if (callbacks != nullptr) session->callbacks = *callbacks;

    deskhubp::TerminalViewerConfig config;
    config.host = server;
    config.hostLabel = deskhub::ui::AddressHost(address);
    config.passcode = passcode != nullptr ? passcode : "";
    config.clientName = deskhubp::SessionDeviceName();
    config.size = deskhub::TermSize{cols, rows};
    config.resumeId = resumeId;
    config.deferOpen = deferOpen;

    DHTermSession* raw = session;
    deskhubp::TerminalViewerCallbacks hooks;
    hooks.onState = [raw](deskhubp::TerminalViewerState state, std::string_view message) {
        if (raw->callbacks.onState == nullptr) return;
        const std::string copy(message);
        raw->callbacks.onState(int32_t(state), copy.c_str(), raw->callbacks.user);
    };
    hooks.onRedraw = [raw] {
        if (raw->callbacks.onRedraw != nullptr) raw->callbacks.onRedraw(raw->callbacks.user);
    };
    hooks.onTrustAsked = [raw](deskhub::TrustVerdict verdict, std::string_view fingerprint) {
        if (raw->callbacks.onTrustAsked == nullptr) {
            raw->viewer.RejectFingerprint();
            return;
        }
        const std::string copy(fingerprint);
        raw->callbacks.onTrustAsked(int32_t(verdict), copy.c_str(), raw->callbacks.user);
    };
    hooks.onSessions = [raw](const deskhub::TermSessionList& sessions) {
        const std::lock_guard<std::mutex> lock(raw->mutex);
        raw->sessions = InfosFor(sessions);
        raw->sessionsKnown = true;
        if (raw->callbacks.onSessions == nullptr) return;
        raw->callbacks.onSessions(
            raw->sessions.data(), uint32_t(raw->sessions.size()), raw->callbacks.user);
    };

    if (!session->viewer.Start(config, std::move(hooks))) {
        delete session;
        return nullptr;
    }
    return session;
}

DHTermSession* dh_term_open(const char* address, const char* passcode, uint16_t cols,
    uint16_t rows, const DHTermCallbacks* callbacks) {
    return StartViewer(address, passcode, cols, rows, 0, false, callbacks);
}

DHTermSession* dh_term_open_deferred(const char* address, const char* passcode, uint16_t cols,
    uint16_t rows, const DHTermCallbacks* callbacks) {
    return StartViewer(address, passcode, cols, rows, 0, true, callbacks);
}

void dh_term_stop(DHTermSession* s) {
    if (s == nullptr) return;
    s->viewer.Stop();
    delete s;
}

int32_t dh_term_state(DHTermSession* s) {
    return s == nullptr ? DHTermIdle : int32_t(s->viewer.State());
}

int dh_term_message(DHTermSession* s, char* out, int capacity) {
    return FillText(out, capacity, s == nullptr ? std::string() : s->viewer.Message());
}

int dh_term_fingerprint(DHTermSession* s, char* out, int capacity) {
    return FillText(out, capacity, s == nullptr ? std::string() : s->viewer.Fingerprint());
}

int32_t dh_term_verdict(DHTermSession* s) {
    return s == nullptr ? int32_t(deskhub::TrustVerdict::Unknown) : int32_t(s->viewer.Verdict());
}

void dh_term_accept_key(DHTermSession* s) {
    if (s != nullptr) s->viewer.AcceptFingerprint();
}

void dh_term_reject_key(DHTermSession* s) {
    if (s != nullptr) s->viewer.RejectFingerprint();
}

bool dh_term_grid(DHTermSession* s, uint32_t scrollOffset, DHTermCell* cells,
    uint32_t cellCapacity, DHTermGrid* outGrid) {
    if (s == nullptr) return false;
    return deskhubp::FillTermGrid(s->viewer.Snapshot(scrollOffset), cells, cellCapacity, outGrid);
}

void dh_term_send_key(DHTermSession* s, int32_t key, uint32_t codepoint, bool shift, bool alt,
    bool ctrl) {
    if (s == nullptr) return;
    deskhub::term::TermKeyEvent event;
    if (!deskhubp::DecodeTermKey(key, codepoint, shift, alt, ctrl, event)) return;
    s->viewer.SendKey(event);
}

void dh_term_send_text(DHTermSession* s, const char* utf8) {
    if (s != nullptr && utf8 != nullptr && *utf8 != '\0') s->viewer.SendText(utf8);
}

void dh_term_open_new(DHTermSession* s) {
    if (s != nullptr) s->viewer.OpenNew();
}

void dh_term_resume(DHTermSession* s, uint32_t termId) {
    if (s != nullptr) s->viewer.ResumeSession(termId);
}

void dh_term_close_session(DHTermSession* s, uint32_t termId) {
    if (s != nullptr) s->viewer.CloseSession(termId);
}

void dh_term_request_sessions(DHTermSession* s) {
    if (s != nullptr) s->viewer.RequestSessions();
}

bool dh_term_sessions_known(DHTermSession* s) {
    if (s == nullptr) return false;
    const std::lock_guard<std::mutex> lock(s->mutex);
    return s->sessionsKnown;
}

uint32_t dh_term_session_count(DHTermSession* s) {
    if (s == nullptr) return 0;
    const std::lock_guard<std::mutex> lock(s->mutex);
    return uint32_t(s->sessions.size());
}

bool dh_term_session_info(DHTermSession* s, uint32_t index, DHTermSessionInfo* out) {
    if (s == nullptr || out == nullptr) return false;
    const std::lock_guard<std::mutex> lock(s->mutex);
    if (index >= s->sessions.size()) return false;
    *out = s->sessions[index];
    return true;
}

void dh_term_resize(DHTermSession* s, uint16_t cols, uint16_t rows) {
    if (s != nullptr) s->viewer.Resize(deskhub::TermSize{cols, rows});
}
