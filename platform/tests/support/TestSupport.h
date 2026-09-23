#pragma once
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"

inline constexpr const char* kTestPasscode = "0417";

inline bool ForgetHostIdentity() {
    deskhubp::RemoveAppDataFile(deskhubp::kHostCertFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    return deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName).empty();
}

extern int g_failures;
void Check(bool ok, const char* what);
