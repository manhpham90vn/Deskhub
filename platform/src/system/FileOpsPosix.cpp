#include "deskhubp/system/FileOps.h"

#include <cerrno>
#include <fcntl.h>
#include <system_error>
#include <unistd.h>

namespace deskhubp {

namespace {

constexpr mode_t kNewFileMode = 0644;

MoveOutcome CheckThenRename(const std::filesystem::path& from,
    const std::filesystem::path& to) {
    std::error_code ec;
    if (std::filesystem::exists(to, ec)) return MoveOutcome::TargetExists;
    std::filesystem::rename(from, to, ec);
    return ec ? MoveOutcome::Failed : MoveOutcome::Moved;
}

}

bool CreateNewFile(const std::filesystem::path& path) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, kNewFileMode);
    if (fd < 0) return false;
    ::close(fd);
    return true;
}

MoveOutcome MoveWithoutReplacing(const std::filesystem::path& from,
    const std::filesystem::path& to) {
    if (::link(from.c_str(), to.c_str()) == 0) {
        ::unlink(from.c_str());
        return MoveOutcome::Moved;
    }
    if (errno == EEXIST) return MoveOutcome::TargetExists;
    return CheckThenRename(from, to);
}

}
