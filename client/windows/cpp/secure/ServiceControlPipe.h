#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <memory>

#include "deskhub/session/host/SecureDesktopWire.h"
#include "secure/OverlappedPipe.h"

std::unique_ptr<OverlappedPipe> ConnectSecureDesktopService(deskhub::SecureDesktopRequest request);
bool AwaitSecureDesktopAck(OverlappedPipe& control);
std::unique_ptr<OverlappedPipe> TakeoverUnattendedHost();
bool SendUnattendedConfig(const deskhub::UnattendedConfig& config);
bool SecureDesktopServiceInstalled();
