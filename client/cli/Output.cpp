#include "Output.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <utility>

#include "deskhub/ui/Strings.h"

namespace deskhubcli {

namespace {

constexpr size_t kColumnGap = 2;

size_t DisplayWidth(std::string_view text) {
    size_t width = 0;
    for (char c : text)
        if ((uint8_t(c) & 0xC0) != 0x80) ++width;
    return width;
}

}

void Table::Row(std::vector<std::string> cells) {
    rows_.push_back(std::move(cells));
}

void Table::Print() const {
    std::vector<size_t> widths;
    for (const std::vector<std::string>& row : rows_) {
        if (widths.size() < row.size()) widths.resize(row.size(), 0);
        for (size_t i = 0; i < row.size(); ++i)
            widths[i] = std::max(widths[i], DisplayWidth(row[i]));
    }

    for (const std::vector<std::string>& row : rows_) {
        std::string line;
        for (size_t i = 0; i < row.size(); ++i) {
            line += row[i];
            if (i + 1 == row.size()) break;
            line.append(widths[i] - DisplayWidth(row[i]) + kColumnGap, ' ');
        }
        PrintLine(line);
    }
}

void PrintLine(std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fputc('\n', stdout);
}

void PrintError(std::string_view text) {
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fputc('\n', stderr);
}

void PrintConnectFailure(std::string_view text) {
    PrintError(text);
    if (text == deskhub::ui::kAuthNotPaired) PrintError(deskhub::ui::kAuthNotPairedCliHint);
}

}
