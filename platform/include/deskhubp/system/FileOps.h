#pragma once
#include <filesystem>

namespace deskhubp {

enum class MoveOutcome {
    Moved,
    TargetExists,
    Failed,
};

bool CreateNewFile(const std::filesystem::path& path);
MoveOutcome MoveWithoutReplacing(const std::filesystem::path& from,
    const std::filesystem::path& to);

}
