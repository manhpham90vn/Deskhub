#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deskhub::ui {

struct ShellPickerRow {
    uint32_t termId = 0;
    std::string label{};
    std::string detail{};
    bool resumable = false;
    bool closable = false;

    bool operator==(const ShellPickerRow&) const = default;
};

std::vector<ShellPickerRow> BuildShellPickerRows(const TermSessionList& sessions);

std::string ShellPickerLine(const ShellPickerRow& row);

bool AnyShellResumable(const std::vector<ShellPickerRow>& rows);

}
