#pragma once
#include <memory>

#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhub/ui/UiSettings.h"

bool UnattendedDaemonInstalled();
std::shared_ptr<void> TakeoverUnattendedHost();
bool PushUnattendedAccess(const deskhub::ui::UiSettings& settings);
void SyncUnattendedAccess(const deskhub::ui::UiSettings& settings);
