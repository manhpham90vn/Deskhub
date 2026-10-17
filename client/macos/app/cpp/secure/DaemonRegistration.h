#pragma once

enum class DaemonStatus {
    NotRegistered,
    Enabled,
    RequiresApproval,
    NotFound,
};

bool RegisterUnattendedDaemon(bool on);
DaemonStatus UnattendedDaemonStatus();
void OpenLoginItemsSettings();
