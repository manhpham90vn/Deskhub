#pragma once
#include <gtk/gtk.h>

#include <cstdint>
#include <string>

namespace deskhubp {
class TerminalHost;
}

struct TerminalLaunch {
    std::string address{};
    std::string clientName{};
};

bool OpenTerminalWindow(GtkWindow* parent, const TerminalLaunch& launch);
bool OpenHostTerminalWindow(GtkWindow* parent, deskhubp::TerminalHost& host, uint32_t termId);
