#pragma once
#include <wx/wx.h>

#include <cstdint>
#include <string>

namespace deskhubp {
class TerminalHost;
}

struct TerminalLaunch {
    std::string address{};
    std::string clientName{};
};

bool OpenTerminalWindow(wxWindow* parent, const TerminalLaunch& launch);
bool OpenHostTerminalWindow(wxWindow* parent, deskhubp::TerminalHost& host, uint32_t termId);
