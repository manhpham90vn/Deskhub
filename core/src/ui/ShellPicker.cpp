#include "deskhub/ui/ShellPicker.h"

#include "deskhub/media/SourceLabel.h"
#include "deskhub/ui/Strings.h"

#include <algorithm>

namespace deskhub::ui {

namespace {

std::string DetailFor(const TermSessionEntry& entry) {
    if (entry.state == TerminalState::Local) return kTerminalLocalElsewhere;
    std::string detail =
        entry.clientName.empty() ? std::string(kTerminalUnnamedClient) : entry.clientName;
    detail += " ";
    detail += entry.state == TerminalState::Detached ? kTerminalDetached : kTerminalInUse;
    return detail;
}

}

std::vector<ShellPickerRow> BuildShellPickerRows(const TermSessionList& sessions) {
    std::vector<ShellPickerRow> rows;
    rows.reserve(sessions.sessions.size());
    for (const TermSessionEntry& entry : sessions.sessions) {
        if (entry.termId == 0) continue;
        ShellPickerRow row;
        row.termId = entry.termId;
        row.label = "#" + std::to_string(entry.termId) + "  " +
                    media::SourceSizeLabel(entry.size.cols, entry.size.rows);
        row.detail = DetailFor(entry);
        row.resumable = entry.state == TerminalState::Detached;
        row.closable = entry.state != TerminalState::Local;
        rows.push_back(std::move(row));
    }
    return rows;
}

std::string ShellPickerLine(const ShellPickerRow& row) {
    return row.label + "  " + row.detail;
}

bool AnyShellResumable(const std::vector<ShellPickerRow>& rows) {
    return std::any_of(rows.begin(), rows.end(),
        [](const ShellPickerRow& row) { return row.resumable; });
}

}
