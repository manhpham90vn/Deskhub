#pragma once
#include "deskhub/session/FileTransfer.h"
#include "deskhub/session/TerminalSession.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/host/FileHost.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/system/FileStore.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace deskhubp {

struct ShareBanner {
    bool screenSharing = false;
    bool hosting = false;
    uint16_t port = 0;
    bool viewOnly = false;
    std::string passcodeNote;
    std::string bindWarning;
};

class ShareController {
public:
    struct Hooks {
        std::function<void(const std::string& message)> onError;
        std::function<void(std::function<void()>)> postToUi;
        std::function<bool(uint32_t termId)> openLocalTerminal;
        std::function<void()> onRowsChanged;
        std::function<void()> onBannerChanged;
        std::function<void()> onNothingLeftShared;
    };

    void SetHooks(Hooks hooks) {
        hooks_ = std::move(hooks);
    }

    SharingHost& sharingHost() {
        return sharingHost_;
    }

    TerminalHost& terminalHost() {
        return terminalHost_;
    }

    const TerminalHost& terminalHost() const {
        return terminalHost_;
    }

    FileHost& fileHost() {
        return fileHost_;
    }

    const FileHost& fileHost() const {
        return fileHost_;
    }

    const std::vector<deskhub::TerminalRecord>& shells() const {
        return shells_;
    }

    const std::vector<deskhub::TransferRecord>& transfers() const {
        return transfers_;
    }

    void StartTerminalShare() {
        if (terminalHost_.Running()) return;

        TerminalHostCallbacks callbacks;
        callbacks.onSessionsChanged = [this] {
            hooks_.postToUi([this] {
                RefreshShells();
                hooks_.onRowsChanged();
            });
        };

        if (!terminalHost_.Start(sharingHost_.Socket(), std::string(), std::move(callbacks))) {
            hooks_.onError(deskhub::ui::kShareStartFailed);
            return;
        }
        RefreshShells();
    }

    void StopTerminalShare() {
        if (!terminalHost_.Running()) return;
        terminalHost_.Stop();
        shells_.clear();
    }

    bool StartFileShare(const std::filesystem::path& folder) {
        if (fileHost_.Running()) return true;

        if (!fileHost_.Start(sharingHost_.Socket(), folder, FileHostCallbacks{})) {
            hooks_.onError(deskhub::ui::TransferFolderUnusable(PathText(folder)));
            return false;
        }
        RefreshTransfers();
        return true;
    }

    void StopFileShare() {
        if (!fileHost_.Running()) return;
        fileHost_.Stop();
        transfers_.clear();
    }

    void StopTerminalRow(bool screenSharing) {
        StopTerminalShare();
        FinishRowRemoval(screenSharing || fileHost_.Running());
    }

    void StopFilesRow(bool screenSharing) {
        StopFileShare();
        FinishRowRemoval(screenSharing || terminalHost_.Running());
    }

    void RefreshShells() {
        shells_ = terminalHost_.Running() ? terminalHost_.Sessions()
                                          : std::vector<deskhub::TerminalRecord>{};
    }

    void RefreshTransfers() {
        transfers_ = fileHost_.Running() ? fileHost_.Transfers()
                                         : std::vector<deskhub::TransferRecord>{};
    }

    void KickShell(uint32_t termId) {
        if (!terminalHost_.Running()) return;
        terminalHost_.KickSession(termId);
    }

    void StopAndAttachShell(uint32_t termId) {
        if (!terminalHost_.AttachLocal(termId)) return;
        RefreshShells();
        hooks_.onRowsChanged();
        if (!hooks_.openLocalTerminal(termId)) KickShell(termId);
    }

    std::string BannerText(const ShareBanner& banner) const {
        std::string status = deskhub::ui::ShareSummaryLine(banner.screenSharing,
            terminalHost_.Running(), fileHost_.Running(), banner.port);
        if (fileHost_.Running())
            status += "\n" + deskhub::ui::TransferFolderNote(PathText(fileHost_.Directory()));
        if (!banner.passcodeNote.empty()) status += "\n" + banner.passcodeNote;
        if (banner.screenSharing && banner.viewOnly)
            status += std::string("\n") + deskhub::ui::kViewOnlyNote;
        if (banner.hosting && !banner.bindWarning.empty()) status += "\n" + banner.bindWarning;
        return status;
    }

private:
    void FinishRowRemoval(bool anythingLeft) {
        if (!anythingLeft) {
            hooks_.onNothingLeftShared();
            return;
        }
        hooks_.onBannerChanged();
        hooks_.onRowsChanged();
    }

    Hooks hooks_;
    SharingHost sharingHost_;
    TerminalHost terminalHost_;
    FileHost fileHost_;
    std::vector<deskhub::TerminalRecord> shells_;
    std::vector<deskhub::TransferRecord> transfers_;
};

}
