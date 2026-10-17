#import <Foundation/Foundation.h>

#include "LoginWindowAgent.h"

#include <string>

#include <mach-o/dyld.h>
#include <sys/stat.h>
#include <unistd.h>

#include "deskhubp/diag/Log.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr mode_t kAgentPlistMode = 0644;

std::string SelfExecutablePath() {
    char path[PATH_MAX] = {};
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) return {};
    char resolved[PATH_MAX] = {};
    return realpath(path, resolved) ? std::string(resolved) : std::string(path);
}

NSDictionary* AgentPlist(const std::string& daemonPath) {
    return @{
        @"Label" : @(UnattendedAgentLabel().c_str()),
        @"ProgramArguments" : @[ @(daemonPath.c_str()), @(kUnattendedHostArg) ],
        @"LimitLoadToSessionType" : @"LoginWindow",
        @"RunAtLoad" : @YES,
        @"KeepAlive" : @{@"SuccessfulExit" : @NO},
        @"ProcessType" : @"Interactive",
    };
}

}

bool WriteLoginWindowAgent() {
    const std::string daemonPath = SelfExecutablePath();
    if (daemonPath.empty()) return false;
    NSError* error = nil;
    NSData* data = [NSPropertyListSerialization dataWithPropertyList:AgentPlist(daemonPath)
                                                              format:NSPropertyListXMLFormat_v1_0
                                                             options:0
                                                               error:&error];
    if (!data) {
        LOGE("[Daemon] Could not build the login window agent plist: %s",
            error.localizedDescription.UTF8String);
        return false;
    }
    const std::string path = UnattendedAgentPlistPath();
    if (![data writeToFile:@(path.c_str()) options:NSDataWritingAtomic error:&error]) {
        LOGE("[Daemon] Could not write %s: %s", path.c_str(),
            error.localizedDescription.UTF8String);
        return false;
    }
    if (chmod(path.c_str(), kAgentPlistMode) != 0) {
        LOGE("[Daemon] Could not set the mode of %s.", path.c_str());
        return false;
    }
    LOGI("[Daemon] Login window agent written to %s.", path.c_str());
    return true;
}

bool RemoveLoginWindowAgent() {
    const std::string path = UnattendedAgentPlistPath();
    if (unlink(path.c_str()) != 0 && errno != ENOENT) {
        LOGW("[Daemon] Could not remove %s.", path.c_str());
        return false;
    }
    return true;
}
