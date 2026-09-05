#pragma once
#include <string>

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/TrustStoreFile.h"

inline constexpr const char* kTestPasscode = "0417";

extern int g_failures;
void Check(bool ok, const char* what);

struct SavedIdentity {
    std::string cert{};
    std::string key{};
    std::string trust{};

    SavedIdentity()
        : cert(deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName)),
          key(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName)),
          trust(deskhubp::ReadAppDataFile(deskhubp::kTrustStoreFileName)) {}

    SavedIdentity(const SavedIdentity&) = delete;
    SavedIdentity& operator=(const SavedIdentity&) = delete;

    ~SavedIdentity() {
        Restore(deskhubp::kHostCertFileName, cert);
        Restore(deskhubp::kHostKeyFileName, key);
        Restore(deskhubp::kTrustStoreFileName, trust);
    }

private:
    static void Restore(const char* fileName, const std::string& saved) {
        if (saved.empty())
            deskhubp::RemoveAppDataFile(fileName);
        else
            deskhubp::WriteAppDataFile(fileName, saved);
    }
};

struct ForgottenHost {
    std::string endpoint{};

    explicit ForgottenHost(std::string host) : endpoint(std::move(host)) {
        deskhubp::ForgetTrustedHost(endpoint);
    }

    ForgottenHost(const ForgottenHost&) = delete;
    ForgottenHost& operator=(const ForgottenHost&) = delete;

    ~ForgottenHost() {
        deskhubp::ForgetTrustedHost(endpoint);
    }
};
