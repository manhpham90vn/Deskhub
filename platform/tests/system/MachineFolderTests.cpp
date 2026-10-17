#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/diag/LogFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/MachineFolder.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {

struct RestoreDirs {
    std::string appData = deskhubp::AppDataDirRef();
    std::string config = deskhubp::ConfigDirRef();

    ~RestoreDirs() {
        deskhubp::SetAppDataDir(appData);
        deskhubp::SetConfigDir(config);
    }
};

void TestEnterCreatesAPrivateFolder() {
    std::printf("[machine] entering the machine folder creates it for this user alone...\n");
    const RestoreDirs restore;
    const auto root = UniqueTempDir("deskhub-machine-");
    const std::string folder = (root / "machine").string();
    Check(deskhubp::EnterMachineFolder(folder), "a missing folder is created on the way in");
    Check(deskhubp::ConfigDir() == folder && deskhubp::LogDir() == folder,
        "config files and logs both land in the machine folder");
    Check(deskhubp::WriteAppDataFile("probe.txt", "x") &&
              std::filesystem::is_regular_file(root / "machine" / "probe.txt"),
        "app data writes go there too");
#ifndef _WIN32
    struct stat mode{};
    Check(::stat(folder.c_str(), &mode) == 0 && (mode.st_mode & 0777) == 0700,
        "nobody else on the machine can list it");
#endif
    Check(deskhubp::WipeMachineFolder(folder) && !std::filesystem::exists(root / "machine"),
        "wiping removes the folder and everything in it");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

void TestEnabledMeansAKeyIsPresent() {
    std::printf("[machine] access before sign-in is on exactly while a host key is kept...\n");
    const RestoreDirs restore;
    const auto root = UniqueTempDir("deskhub-machine-key-");
    const std::string folder = (root / "machine").string();
    Check(!deskhubp::UnattendedEnabled(folder), "a folder that does not exist is off");
    Check(deskhubp::EnterMachineFolder(folder), "the folder is entered");
    Check(!deskhubp::UnattendedEnabled(folder), "an empty folder is off");
    Check(deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, "pem"), "a key is copied in");
    Check(deskhubp::UnattendedEnabled(folder), "with a key present it is on");
    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    Check(!deskhubp::UnattendedEnabled(folder), "removing the key switches it off again");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

void TestRefusesWhatIsNotAFolder() {
    std::printf("[machine] a path that is not a folder is refused...\n");
    const RestoreDirs restore;
    Check(!deskhubp::EnterMachineFolder(""), "an empty path is refused");
    Check(!deskhubp::UnattendedEnabled("") && !deskhubp::WipeMachineFolder(""),
        "and neither read nor wiped");
    const auto root = UniqueTempDir("deskhub-machine-file-");
    std::filesystem::create_directories(root);
    const auto file = root / "plain";
    std::ofstream(file) << "not a folder";
    Check(!deskhubp::EnterMachineFolder(file.string()), "a regular file cannot be entered");
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

}

void RunMachineFolderTests() {
    TestEnterCreatesAPrivateFolder();
    TestEnabledMeansAKeyIsPresent();
    TestRefusesWhatIsNotAFolder();
}
