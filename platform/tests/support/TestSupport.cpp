#include "support/TestSupport.h"

#include "deskhubp/system/Random.h"

#include <array>
#include <cstdio>
#include <system_error>

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL: %s\n", what);
    }
}

std::filesystem::path UniqueTempDir(const std::string& prefix) {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) return {};
    std::string name = prefix;
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    return std::filesystem::temp_directory_path() / name;
}

IsolatedAppData::IsolatedAppData(const std::string& prefix) : dir(UniqueTempDir(prefix)) {
    if (!dir.empty()) deskhubp::SetAppDataDir(dir.string());
}

IsolatedAppData::~IsolatedAppData() {
    deskhubp::SetAppDataDir(previous);
    std::error_code error;
    if (!dir.empty()) std::filesystem::remove_all(dir, error);
}
