#include "deskhub/diag/LogRetention.h"

#include <algorithm>

namespace deskhub {

namespace {

constexpr std::string_view kLogPrefix = "deskhub-";
constexpr std::string_view kLogSuffix = ".log";
constexpr size_t kDateDigits = 8;
constexpr size_t kTimeDigits = 6;

bool DigitsAt(std::string_view text, size_t from, size_t count) {
    if (count == 0 || from + count > text.size()) return false;
    const std::string_view digits = text.substr(from, count);
    return std::all_of(digits.begin(), digits.end(), [](char c) { return c >= '0' && c <= '9'; });
}

}

bool IsSessionLogName(std::string_view name) {
    if (!name.starts_with(kLogPrefix) || !name.ends_with(kLogSuffix)) return false;
    const size_t date = kLogPrefix.size();
    const size_t time = date + kDateDigits + 1;
    const size_t pid = time + kTimeDigits + 1;
    if (pid + kLogSuffix.size() > name.size()) return false;
    return DigitsAt(name, date, kDateDigits) && name[date + kDateDigits] == '-' &&
           DigitsAt(name, time, kTimeDigits) && name[time + kTimeDigits] == '-' &&
           DigitsAt(name, pid, name.size() - kLogSuffix.size() - pid);
}

std::vector<std::string> SessionLogsToPrune(std::vector<std::string> names, size_t keep) {
    std::erase_if(names, [](const std::string& name) { return !IsSessionLogName(name); });
    if (names.size() <= keep) return {};
    std::sort(names.begin(), names.end());
    names.resize(names.size() - keep);
    return names;
}

}
