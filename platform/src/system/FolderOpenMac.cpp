#include "deskhubp/system/FolderOpen.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <string>

#include "deskhubp/diag/Log.h"

namespace deskhubp {

bool OpenFolder(const std::filesystem::path& folder) {
    const std::string path = folder.string();
    const pid_t middle = fork();
    if (middle < 0) {
        LOGW("[FolderOpen] fork failed: %d", errno);
        return false;
    }
    if (middle == 0) {
        if (fork() == 0) {
            execlp("open", "open", path.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        _exit(0);
    }
    waitpid(middle, nullptr, 0);
    return true;
}

}
