#include "deskhubp/ffi/ShareFfi.h"

#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "deskhub/media/QualityPreset.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/HostRows.h"
#include "deskhubp/ffi/DiscoveryFfi.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/ffi/TermGridFill.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/host/FileHost.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/system/FileStore.h"
#include "deskhubp/system/FolderOpen.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

std::unique_ptr<SharingHost> g_agent;
std::unique_ptr<deskhubp::TerminalHost> g_terminal;
std::unique_ptr<deskhubp::FileHost> g_files;
std::mutex g_agentMutex;

std::mutex g_audioMutex;
SharingHost* g_audioTarget = nullptr;

void PublishAudioTarget(SharingHost* target) {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    g_audioTarget = target;
}
uint16_t g_port = deskhub::kDeskhubPort;

char g_errorBuf[512];

ShareSource ToShareSource(const DHShareSource& s) {
    ShareSource a;
    a.targetId = s.id;
    a.name = s.name;
    a.width = s.width;
    a.height = s.height;
    return a;
}

std::filesystem::path FilesFolder() {
    std::filesystem::path folder;
    {
        std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
        if (lk.owns_lock() && g_files) folder = g_files->Directory();
    }
    if (!folder.empty()) return folder;
    const std::string chosen = deskhubp::LoadUiSettings().transferDir;
    return chosen.empty() ? deskhubp::DefaultTransferDir() : deskhubp::FfiPath(chosen.c_str());
}

}

DHShareDefaults dh_share_default_options(void) {
    const ShareOptions defaults;
    return DHShareDefaults{defaults.fps, defaults.bitrateMbps, defaults.maxDim};
}

int dh_share_quality_presets(DHQualityPreset* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const auto& presets = deskhub::media::kQualityPresets;
    const int count = int(presets.size()) < capacity ? int(presets.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        deskhubp::CopyToBuf(out[i].label, sizeof(out[i].label), presets[size_t(i)].label);
        out[i].maxDim = presets[size_t(i)].maxDim;
    }
    return count;
}

int dh_share_list_sources(DHShareSource* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const std::vector<deskhub::media::ShareSource> src = deskhubp::ListDisplays();
    const int count = int(src.size()) < capacity ? int(src.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        out[i].id = uint32_t(src[size_t(i)].targetId);
        out[i].width = src[size_t(i)].width;
        out[i].height = src[size_t(i)].height;
        deskhubp::CopyToBuf(out[i].name, sizeof(out[i].name), src[size_t(i)].name);
    }
    return count;
}

bool dh_share_start(const DHShareSource* sources, int count, uint32_t fps, uint32_t bitrate_mbps,
    uint32_t max_dim, uint16_t port, bool allow_input, const char* passcode, bool terminal,
    bool files) {
    if ((!sources || count <= 0) && !terminal && !files) return false;

    std::vector<ShareSource> list;
    if (sources && count > 0) {
        list.reserve(size_t(count));
        for (int i = 0; i < count; ++i) list.push_back(ToShareSource(sources[i]));
    }

    ShareOptions opt;
    if (fps) opt.fps = fps;
    if (bitrate_mbps) opt.bitrateMbps = bitrate_mbps;
    opt.maxDim = max_dim;
    if (port) opt.port = port;
    opt.allowInput = allow_input;
    opt.terminal = terminal;
    opt.files = files;
    const std::string typedPasscode = passcode ? passcode : "";
    opt.passcode = typedPasscode.empty() || deskhub::IsValidPasscode(typedPasscode)
                       ? typedPasscode
                       : deskhubp::HostPasscode();
    std::filesystem::path transferDir;
    {
        const deskhub::ui::UiSettings stored = deskhubp::LoadUiSettings();
        transferDir = stored.transferDir.empty() ? deskhubp::DefaultTransferDir()
                                                 : deskhubp::FfiPath(stored.transferDir.c_str());
        opt.bindIp = stored.bindIp;
        opt.clipboardSync = stored.clipboardSync;
        opt.audio = stored.shareAudio;
        opt.deviceName = stored.deviceName;
        opt.allowNewPairings = stored.allowNewPairings;
    }

    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_agent) {
        PublishAudioTarget(nullptr);
        if (g_files) g_files->Stop();
        if (g_terminal) g_terminal->Stop();
        g_agent->Stop();
        g_agent.reset();
    }
    g_agent = std::make_unique<SharingHost>();
    if (!g_terminal) g_terminal = std::make_unique<deskhubp::TerminalHost>();
    if (!g_files) g_files = std::make_unique<deskhubp::FileHost>();
    g_agent->SetTerminal(g_terminal.get());
    g_agent->SetFiles(g_files.get());
    if (!g_agent->Start(list, opt)) {
        deskhubp::CopyToBuf(g_errorBuf, sizeof(g_errorBuf), g_agent->LastError());
        g_agent.reset();
        return false;
    }
    PublishAudioTarget(g_agent.get());
    g_port = opt.port;
    if (terminal)
        g_terminal->Start(g_agent->Socket(), std::string(), deskhubp::TerminalHostCallbacks{});
    if (files) g_files->Start(g_agent->Socket(), transferDir, deskhubp::FileHostCallbacks{});
    g_errorBuf[0] = '\0';
    return true;
}

void dh_share_stop(void) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_agent) {
        PublishAudioTarget(nullptr);
        if (g_files) g_files->Stop();
        if (g_terminal) g_terminal->Stop();
        g_agent->Stop();
        g_agent.reset();
    }
}

void dh_share_offer_audio(const int16_t* pcm, int samples) {
    if (!pcm || samples <= 0) return;
    std::lock_guard<std::mutex> lk(g_audioMutex);
    if (!g_audioTarget) return;
    g_audioTarget->OfferAudio(std::span<const int16_t>(pcm, size_t(samples)));
}

bool dh_share_audio_running(void) {
    std::lock_guard<std::mutex> lk(g_audioMutex);
    return g_audioTarget != nullptr && g_audioTarget->audioRunning();
}

bool dh_share_files_active(void) {
    std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
    return lk.owns_lock() && g_files && g_files->Running();
}

void dh_share_stop_files(void) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_files) g_files->Stop();
}

int dh_share_files_dir(char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    out[0] = '\0';
    const std::filesystem::path folder = FilesFolder();
    const std::u8string text = folder.u8string();
    deskhubp::CopyToBuf(out, size_t(capacity), std::string(text.begin(), text.end()));
    return int(std::strlen(out));
}

bool dh_share_open_files_folder(void) {
    return deskhubp::OpenFolder(FilesFolder());
}

void dh_share_kick_shell(uint32_t term_id) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_terminal) g_terminal->KickSession(term_id);
}

void dh_share_stop_terminal(void) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_terminal) g_terminal->Stop();
}

bool dh_share_attach_shell(uint32_t term_id) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    return g_terminal && g_terminal->AttachLocal(term_id);
}

bool dh_share_local_shell_alive(uint32_t term_id) {
    std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
    return lk.owns_lock() && g_terminal && g_terminal->LocalAlive(term_id);
}

void dh_share_close_local_shell(uint32_t term_id) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_terminal) g_terminal->CloseLocal(term_id);
}

bool dh_share_local_grid(uint32_t term_id, uint32_t scrollOffset, DHTermCell* cells,
    uint32_t cellCapacity, DHTermGrid* outGrid) {
    deskhub::term::TerminalSnapshot shot;
    {
        std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
        if (!lk.owns_lock() || !g_terminal) return false;
        shot = g_terminal->LocalSnapshot(term_id, scrollOffset);
    }
    return deskhubp::FillTermGrid(shot, cells, cellCapacity, outGrid);
}

void dh_share_local_send_key(uint32_t term_id, int32_t key, uint32_t codepoint, bool shift, bool alt,
    bool ctrl) {
    deskhub::term::TermKeyEvent event;
    if (!deskhubp::DecodeTermKey(key, codepoint, shift, alt, ctrl, event)) return;
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_terminal) g_terminal->SendLocalKey(term_id, event);
}

void dh_share_local_send_text(uint32_t term_id, const char* utf8) {
    if (utf8 == nullptr || *utf8 == '\0') return;
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_terminal) g_terminal->SendLocalText(term_id, utf8);
}

void dh_share_local_resize(uint32_t term_id, uint16_t cols, uint16_t rows) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_terminal) g_terminal->ResizeLocal(term_id, deskhub::TermSize{cols, rows});
}

int dh_share_take_pairing_requests(DHPairingRequest* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::vector<deskhubp::PairingRequest> requests;
    {
        std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
        if (!lk.owns_lock() || !g_agent) return 0;
        requests = g_agent->TakePairingRequests(size_t(capacity));
    }
    const int count = int(requests.size()) < capacity ? int(requests.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        out[i].addrPacked = requests[size_t(i)].addrPacked;
        deskhubp::CopyToBuf(out[i].shortKey, sizeof(out[i].shortKey),
            requests[size_t(i)].shortKey);
        deskhubp::CopyToBuf(out[i].name, sizeof(out[i].name), requests[size_t(i)].name);
    }
    return count;
}

void dh_share_answer_pairing(uint64_t addr_packed, bool allowed) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_agent) g_agent->AnswerPairing(addr_packed, allowed);
}

void dh_share_stop_source(uint8_t source_id) {
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_agent) g_agent->StopSource(source_id);
}

void dh_share_kick_viewer(uint8_t source_id, const char* viewer_addr) {
    if (!viewer_addr) return;
    NetAddr addr{};
    if (!ParseNetAddr(viewer_addr, addr)) return;
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_agent) g_agent->KickViewer(source_id, addr.Pack());
}

bool dh_share_running(void) {
    std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
    return lk.owns_lock() && g_agent && g_agent->running();
}

int dh_share_rows(DHHostRow* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::vector<ShareSourceStatus> sources;
    std::vector<deskhub::TerminalRecord> shells;
    std::vector<deskhub::TransferRecord> transfers;
    bool terminalOn = false;
    bool filesOn = false;
    std::string folder;
    uint16_t port = deskhub::kDeskhubPort;
    {
        std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
        if (!lk.owns_lock() || !g_agent) return 0;
        sources = g_agent->Status();
        terminalOn = g_terminal && g_terminal->Running();
        if (terminalOn) shells = g_terminal->Sessions();
        filesOn = g_files && g_files->Running();
        if (filesOn) {
            transfers = g_files->Transfers();
            const std::u8string text = g_files->Directory().u8string();
            folder.assign(text.begin(), text.end());
        }
        port = g_port;
    }

    const std::vector<deskhub::ui::HostRow> rows =
        deskhub::ui::BuildHostRows(sources, terminalOn, shells, filesOn, transfers);
    int written = 0;
    for (const deskhub::ui::HostRow& row : rows) {
        if (written >= capacity) break;
        deskhub::ui::HostRowCells cells;
        if (row.terminal) {
            cells = deskhub::ui::TerminalRowText(row, port, shells);
        } else if (row.files) {
            cells = deskhub::ui::FilesRowText(row, folder, transfers);
        } else {
            const ShareSourceStatus* source =
                deskhub::ui::FindHostSource(sources, row.sourceId);
            if (!source) continue;
            cells = deskhub::ui::HostRowText(row, *source);
        }
        DHHostRow& slot = out[written++];
        slot.viewer = row.viewer;
        slot.terminal = row.terminal;
        slot.files = row.files;
        slot.sourceId = row.sourceId;
        slot.termId = row.termId;
        slot.shellState = uint8_t(row.shellState);
        slot.online = cells.online;
        deskhubp::CopyToBuf(slot.viewerAddr, sizeof(slot.viewerAddr), row.viewerAddr);
        deskhubp::CopyToBuf(slot.source, sizeof(slot.source), cells.source);
        deskhubp::CopyToBuf(slot.size, sizeof(slot.size), cells.size);
        deskhubp::CopyToBuf(slot.viewers, sizeof(slot.viewers), cells.viewers);
        deskhubp::CopyToBuf(slot.client, sizeof(slot.client), cells.client);
        deskhubp::CopyToBuf(slot.capture, sizeof(slot.capture), cells.capture);
        deskhubp::CopyToBuf(slot.send, sizeof(slot.send), cells.send);
        deskhubp::CopyToBuf(slot.mbps, sizeof(slot.mbps), cells.mbps);
        deskhubp::CopyToBuf(slot.rtt, sizeof(slot.rtt), cells.rtt);
    }
    return written;
}

const char* dh_share_last_error(void) {
    std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
    if (lk.owns_lock() && g_agent)
        deskhubp::CopyToBuf(g_errorBuf, sizeof(g_errorBuf), g_agent->LastError());
    return g_errorBuf;
}

const char* dh_share_local_addresses(void) {
    return dh_local_addresses();
}

void dh_share_clip_offer(const char* text) {
    if (!text || !*text) return;
    std::lock_guard<std::mutex> lk(g_agentMutex);
    if (g_agent) g_agent->OfferLocalClipboard(text);
}

int dh_share_clip_take(char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    out[0] = '\0';
    std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
    if (!lk.owns_lock() || !g_agent) return 0;
    const std::optional<std::string> text = g_agent->TakeRemoteClipboard();
    if (!text) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity), *text);
    return int(std::strlen(out));
}

int dh_share_bind_warning(char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    out[0] = '\0';
    std::unique_lock<std::mutex> lk(g_agentMutex, std::try_to_lock);
    if (lk.owns_lock() && g_agent)
        deskhubp::CopyToBuf(out, size_t(capacity), g_agent->BindWarning());
    return int(std::strlen(out));
}
