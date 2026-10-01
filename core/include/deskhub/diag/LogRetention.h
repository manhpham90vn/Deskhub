#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr size_t kKeptSessionLogs = 10;

bool IsSessionLogName(std::string_view name);
std::vector<std::string> SessionLogsToPrune(std::vector<std::string> names, size_t keep);

}
