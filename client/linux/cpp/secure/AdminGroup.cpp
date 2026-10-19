#include "secure/AdminGroup.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

#include <grp.h>
#include <pwd.h>

namespace {

constexpr std::array<const char*, 3> kAdminGroups = {"sudo", "wheel", "admin"};
constexpr size_t kNameBufferBytes = 16384;
constexpr int kMaxGroups = 256;

struct Account {
    std::string name;
    gid_t primaryGroup = 0;
};

std::optional<Account> AccountOf(uid_t uid) {
    passwd entry{};
    passwd* found = nullptr;
    std::vector<char> buffer(kNameBufferBytes);
    if (getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found) != 0 || !found)
        return std::nullopt;
    return Account{entry.pw_name, entry.pw_gid};
}

std::vector<gid_t> GroupsOf(const Account& account) {
    std::vector<gid_t> groups(kMaxGroups);
    int count = kMaxGroups;
    if (getgrouplist(account.name.c_str(), account.primaryGroup, groups.data(), &count) < 0)
        return {};
    groups.resize(size_t(count));
    return groups;
}

std::optional<gid_t> GroupId(const char* name) {
    group entry{};
    group* found = nullptr;
    std::vector<char> buffer(kNameBufferBytes);
    if (getgrnam_r(name, &entry, buffer.data(), buffer.size(), &found) != 0 || !found)
        return std::nullopt;
    return entry.gr_gid;
}

}

bool IsAdministrator(uint32_t uid) {
    if (uid == 0) return true;
    const std::optional<Account> account = AccountOf(uid_t(uid));
    if (!account) return false;
    const std::vector<gid_t> groups = GroupsOf(*account);
    return std::any_of(kAdminGroups.begin(), kAdminGroups.end(), [&groups](const char* name) {
        const std::optional<gid_t> gid = GroupId(name);
        return gid && std::find(groups.begin(), groups.end(), *gid) != groups.end();
    });
}
