#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "deskhub/ui/TransferView.h"

struct FileSendHooks {
    std::function<bool(const std::vector<std::filesystem::path>&)> begin;
    std::function<void()> cancel;
    std::function<deskhub::ui::TransferView()> view;
    std::function<std::string()> error;
};

struct FileSendLaunch {
    std::string address;
    std::string passcode;
    std::string clientName;
};

void RunFileSendWindow(HWND owner, bool modal, const std::string& subtitle,
    const FileSendHooks& hooks);

void RunStandaloneFileSend(const FileSendLaunch& launch);
