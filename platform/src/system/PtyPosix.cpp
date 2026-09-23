#include "deskhubp/system/Pty.h"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

#include "deskhubp/diag/Log.h"

namespace deskhubp {

namespace {

winsize ToWinSize(deskhub::TermSize size) {
    const deskhub::TermSize clamped = deskhub::ClampTermSize(size);
    winsize ws{};
    ws.ws_col = clamped.cols;
    ws.ws_row = clamped.rows;
    return ws;
}

void ChildSetup(const std::string& shell) {
    setenv("TERM", "xterm-256color", 1);
    unsetenv("LINES");
    unsetenv("COLUMNS");
    signal(SIGPIPE, SIG_DFL);
    execl(shell.c_str(), shell.c_str(), "-l", nullptr);
    execl(shell.c_str(), shell.c_str(), nullptr);
    _exit(127);
}

}

struct Pty::Impl {
    int master = -1;
    pid_t child = -1;
    bool exited = false;
    int exitCode = 0;

    void Reap(bool wait) {
        if (child <= 0 || exited) return;
        int status = 0;
        const pid_t got = waitpid(child, &status, wait ? 0 : WNOHANG);
        if (got != child) return;
        exited = true;
        exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        child = -1;
    }

    void Shutdown() {
        if (child > 0) {
            kill(child, SIGHUP);
            Reap(false);
        }
        if (master >= 0) {
            close(master);
            master = -1;
        }
    }
};

Pty::Pty() : impl_(std::make_unique<Impl>()) {
}

Pty::~Pty() {
    impl_->Shutdown();
}

std::string DefaultShell() {
    const char* fromEnv = getenv("SHELL");
    if (fromEnv != nullptr && *fromEnv != '\0') return fromEnv;
    const passwd* pw = getpwuid(getuid());
    if (pw != nullptr && pw->pw_shell != nullptr && *pw->pw_shell != '\0') return pw->pw_shell;
    return "/bin/sh";
}

bool Pty::Start(const std::string& shell, deskhub::TermSize size) {
    if (impl_->child > 0) return false;
    const std::string command = shell.empty() ? DefaultShell() : shell;
    winsize ws = ToWinSize(size);

    int master = -1;
    const pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        LOGE("pty: forkpty failed (%s)", std::strerror(errno));
        return false;
    }
    if (pid == 0) {
        ChildSetup(command);
        return false;
    }

    const int flags = fcntl(master, F_GETFL, 0);
    if (flags >= 0) fcntl(master, F_SETFL, flags | O_NONBLOCK);

    impl_->master = master;
    impl_->child = pid;
    impl_->exited = false;
    impl_->exitCode = 0;
    return true;
}

bool Pty::Running() const {
    return impl_->master >= 0 && !impl_->exited;
}

int Pty::Read(uint8_t* buf, size_t cap, uint32_t waitMs) {
    if (impl_->master < 0) return -1;
    pollfd fds{};
    fds.fd = impl_->master;
    fds.events = POLLIN;
    const int ready = poll(&fds, 1, int(waitMs));
    if (ready <= 0) {
        impl_->Reap(false);
        return 0;
    }
    const ssize_t got = read(impl_->master, buf, cap);
    if (got > 0) return int(got);
    if (got < 0 && (errno == EAGAIN || errno == EINTR)) return 0;
    impl_->Reap(true);
    impl_->exited = true;
    return -1;
}

bool Pty::Write(std::span<const uint8_t> bytes) {
    if (impl_->master < 0) return false;
    size_t at = 0;
    while (at < bytes.size()) {
        const ssize_t written = write(impl_->master, bytes.data() + at, bytes.size() - at);
        if (written > 0) {
            at += size_t(written);
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        return false;
    }
    return true;
}

bool Pty::Resize(deskhub::TermSize size) {
    if (impl_->master < 0) return false;
    winsize ws = ToWinSize(size);
    return ioctl(impl_->master, TIOCSWINSZ, &ws) == 0;
}

int Pty::ExitCode() const {
    return impl_->exitCode;
}

void Pty::Close() {
    impl_->Shutdown();
}

}
